/* The Dreamcast music: lets the frontend play the Dreamcast version's music (its sound driver and
 * banks on an emulated AICA, AudioCommon/DCMusic.cpp) in place of the GameCube's. The research:
 * ../SkiesOfArcadiaLegends-Decomp/docs/sound.md.
 *
 * The game's sound layer is the Dreamcast one rebuilt on MusyX: music is requested as Dreamcast
 * (bank, data number) pairs, and its frame update (fn_80218678) starts each with one call,
 * sndSeqPlayEx at 0x8021882C (dcm_seq_play). When the frontend has the Dreamcast data for that pair,
 * the sequence starts with every track muted (SND_PLAYPARA_TRACKMUTE, mask 0): it runs as before,
 * so its end, the play list, pause and continue all behave as in the unpatched game, but plays no
 * notes. Every start, and every volume change, pause, continue and stop of that sequence (all calls
 * of those MusyX functions go through the dcm_* wrappers), is posted to a ring the frontend reads
 * (Core/SkiesMusic.cpp), which plays the same on the Dreamcast driver. A paused song (the field music
 * during a battle) is kept beside the current one, so its continue or stop is posted too. The
 * adaptive town music's channel volumes (MIDI controller 7, set every frame) are state, not events:
 * they go to a per-channel table the frontend mirrors. Turning the option on or off mid-song mutes
 * or unmutes the running sequence (dcm_frame) and posts that too.
 *
 * Everything lives in MetroTRK's message buffers (lbl_80319078, unused without a debugger; cave.ld),
 * after trace2d.c's table and before shipdebug.c's word. With no frontend the magic stays 0 and the
 * music is the GameCube's. */

typedef unsigned int u32;
typedef int s32;
typedef unsigned short u16;
typedef unsigned char u8;

#pragma clang section text=".trkcave"

typedef struct {
  u32 flags;
  u32 trackMute[2];
  u16 speed;
  struct {
    u16 time;
    u8 target;
  } volume;
  u8 numSeqVolDef;
  void *seqVolDef;
  u8 numFaded;
  u8 *faded;
} SND_PLAYPARA;

#define SND_PLAYPARA_TRACKMUTE 1u
#define SND_SEQ_ERROR_ID 0xFFFFFFFFu

extern u32 sndSeqPlayEx(u32 sgid, u32 sid, void *arrfile, SND_PLAYPARA *para, u8 studio);
extern void sndSeqVolume(u8 volume, u16 time, u32 seq, u8 mode);
extern void sndSeqPause(u32 seq);
extern void sndSeqContinue(u32 seq);
extern void sndSeqStop(u32 seq);
extern void sndSeqMute(u32 seq, u32 mask1, u32 mask2);
extern int sndSeqSetMidiCtrl(u32 seq, u8 channel, u8 ctrl, u8 value);

/* The shim's bank table: 11 slots of 0x3C bytes, +8 MusyX group id, +0xC first song id, +0x10 state
 * (>= 0 loaded), +0x14 the file name ("/sound/m0231"). */
#define BANKS ((volatile unsigned char *)0x80315120)
#define BANK_SLOTS 11
#define BANK_SIZE 0x3C
/* The music's MusyX sequence id, -1 = none (cleared on pause and at the end). */
#define BGM_SEQ (*(volatile s32 *)0x803477E0)

enum {
  EV_PLAY = 1,     /* a = seq, b = slot << 8 | num, c = muted */
  EV_STOP = 2,     /* a = seq (the current or the paused song) */
  EV_PAUSE = 3,    /* a = seq: it becomes the paused song */
  EV_CONTINUE = 4, /* a = seq, b = slot << 8 | num, c = muted: the paused song is current again */
  EV_VOLUME = 5,   /* a = seq, b = mode << 8 | volume (0..127), c = time in ms */
  EV_UNMUTE = 6,   /* a = seq: the GameCube music plays it again */
  EV_MIDICTRL = 7, /* a = seq, b = channel << 16 | controller << 8 | value (not controller 7) */
};

#define EVENTS 12
struct dcm_box {
  u32 magic;           /* frontend: DCM_MAGIC once it reads the ring */
  u32 enabled;         /* frontend: 1 = mute the GameCube music where allow says so */
  u32 head;            /* events written so far (the ring index is head % EVENTS) */
  s32 seq;             /* the current song's sequence, -1 = none (the frontend sets -1 first) */
  u32 slot_num;        /* its slot << 8 | num */
  u32 muted;           /* whether it was started (or later switched) muted */
  s32 paused_seq;      /* the paused song (MusicPause), -1 = none */
  u32 paused_slot_num;
  u32 paused_muted;
  u32 allow[BANK_SLOTS][2]; /* frontend: per bank slot, the data numbers with Dreamcast data */
  u32 allow_name[BANK_SLOTS]; /* frontend: the slot's file name hash the bits are for */
  u32 vol_set;         /* the channels (bit n) of the current song the game set controller 7 on */
  u8 vol[16];          /* their values */
  u32 events[EVENTS][4];
};

#define DCM_BOX ((volatile struct dcm_box *)0x8031A890)
#define DCM_MAGIC 0x44434D55u /* 'DCMU' */

_Static_assert(sizeof(struct dcm_box) <= 0x8031AA24 - 0x8031A890, "dcm_box runs into shipdebug's word");

/* The file name hash the frontend writes beside a slot's bits (FNV-1a of the name at +0x14), so bits
 * for a bank the slot no longer holds are never used. */
static u32 name_hash(volatile const unsigned char *name) {
  u32 h = 2166136261u;
  for (int i = 0; i < 0x20 && name[i]; ++i)
    h = (h ^ name[i]) * 16777619u;
  return h;
}

static void post(u32 type, u32 a, u32 b, u32 c) {
  volatile struct dcm_box *box = DCM_BOX;
  volatile u32 *e = box->events[box->head % EVENTS];
  e[0] = type;
  e[1] = a;
  e[2] = b;
  e[3] = c;
  box->head = box->head + 1;
}

static int allowed(u32 slot, u32 num) {
  volatile struct dcm_box *box = DCM_BOX;
  if (box->magic != DCM_MAGIC || !box->enabled || slot >= BANK_SLOTS || num >= 64)
    return 0;
  if (box->allow_name[slot] != name_hash(BANKS + slot * BANK_SIZE + 0x14))
    return 0;
  return (box->allow[slot][num >> 5] >> (num & 31)) & 1;
}

/* 0x8021882C: the music update starting a song. */
u32 dcm_seq_play(u32 sgid, u32 sid, void *arrfile, SND_PLAYPARA *para, u8 studio) {
  volatile struct dcm_box *box = DCM_BOX;
  u32 slot = BANK_SLOTS, num = 0;
  for (u32 i = 0; i < BANK_SLOTS; ++i) {
    volatile unsigned char *b = BANKS + i * BANK_SIZE;
    if (*(volatile s32 *)(b + 0x10) >= 0 && *(volatile u32 *)(b + 8) == sgid) {
      slot = i;
      num = sid - *(volatile u32 *)(b + 0xC);
      break;
    }
  }
  const int mute = allowed(slot, num);
  SND_PLAYPARA muted;
  if (mute && !para) {
    muted.flags = SND_PLAYPARA_TRACKMUTE;
    muted.trackMute[0] = 0;
    muted.trackMute[1] = 0;
    para = &muted;
  }
  const u32 seq = sndSeqPlayEx(sgid, sid, arrfile, para, studio);
  if (box->magic == DCM_MAGIC && seq != SND_SEQ_ERROR_ID) {
    box->seq = (s32)seq;
    box->slot_num = slot << 8 | num;
    box->muted = mute;
    box->vol_set = 0;
    post(EV_PLAY, seq, slot << 8 | num, mute);
  }
  return seq;
}

static int current(u32 seq) {
  volatile struct dcm_box *box = DCM_BOX;
  return box->magic == DCM_MAGIC && seq != SND_SEQ_ERROR_ID && (s32)seq == box->seq;
}

static int paused(u32 seq) {
  volatile struct dcm_box *box = DCM_BOX;
  return box->magic == DCM_MAGIC && seq != SND_SEQ_ERROR_ID && (s32)seq == box->paused_seq;
}

void dcm_seq_volume(u8 volume, u16 time, u32 seq, u8 mode) {
  if (current(seq) || paused(seq))
    post(EV_VOLUME, seq, (u32)mode << 8 | volume, time);
  sndSeqVolume(volume, time, seq, mode);
}

/* MusicPause (sound.md): the song steps aside for another (a battle's) and continues after it. */
void dcm_seq_pause(u32 seq) {
  volatile struct dcm_box *box = DCM_BOX;
  if (current(seq)) {
    post(EV_PAUSE, seq, 0, 0);
    box->paused_seq = box->seq;
    box->paused_slot_num = box->slot_num;
    box->paused_muted = box->muted;
    box->seq = -1;
  }
  sndSeqPause(seq);
}

void dcm_seq_continue(u32 seq) {
  volatile struct dcm_box *box = DCM_BOX;
  if (paused(seq)) {
    post(EV_CONTINUE, seq, box->paused_slot_num, box->paused_muted);
    box->seq = box->paused_seq;
    box->slot_num = box->paused_slot_num;
    box->muted = box->paused_muted;
    box->vol_set = 0;
    box->paused_seq = -1;
  }
  sndSeqContinue(seq);
}

void dcm_seq_stop(u32 seq) {
  volatile struct dcm_box *box = DCM_BOX;
  if (current(seq)) {
    post(EV_STOP, seq, 0, 0);
    box->seq = -1;
  } else if (paused(seq)) {
    post(EV_STOP, seq, 0, 0);
    box->paused_seq = -1;
  }
  sndSeqStop(seq);
}

/* The adaptive town / dungeon music (sound.md, "Adaptive music") sets controller 7 of channels 10..14
 * every frame, and 17 beside 14: MusyX channels from 16 up have no Dreamcast equivalent (the driver
 * has 16 per port), so they're left out. */
int dcm_seq_midi_ctrl(u32 seq, u8 channel, u8 ctrl, u8 value) {
  volatile struct dcm_box *box = DCM_BOX;
  if (current(seq) && channel < 16) {
    if (ctrl == 7) {
      box->vol[channel] = value;
      box->vol_set = box->vol_set | 1u << channel;
    } else {
      post(EV_MIDICTRL, seq, (u32)channel << 16 | (u32)ctrl << 8 | value, 0);
    }
  }
  return sndSeqSetMidiCtrl(seq, channel, ctrl, value);
}

/* Once a frame (qol_frame): the option switched while a song plays mutes or unmutes it. */
void dcm_frame(void) {
  volatile struct dcm_box *box = DCM_BOX;
  if (box->magic != DCM_MAGIC || box->seq < 0 || BGM_SEQ != box->seq)
    return;
  const u32 slot = box->slot_num >> 8, num = box->slot_num & 0xFF;
  const u32 mute = allowed(slot, num);
  if (mute == box->muted)
    return;
  box->muted = mute;
  sndSeqMute((u32)box->seq, mute ? 0 : 0xFFFFFFFFu, mute ? 0 : 0xFFFFFFFFu);
  if (mute)
    post(EV_PLAY, (u32)box->seq, box->slot_num, 1);
  else
    post(EV_UNMUTE, (u32)box->seq, 0, 0);
}
