/* dish.h — Pocket Dish: a living petri dish (the dish branch, 2026-10-16).
 *
 * The same split as the tank: this file and dish.c are the world (cells,
 * fields, genomes, the keeper's tools), with no I/O and no OS calls, so they
 * compile unchanged for the sim and the firmware. dish_render.c draws it,
 * dish_ui.c lays the pet's pages over it, dish_brain.c decides.
 *
 * The pet is a STRAIN, not a cell: cells divide and die all the time, the
 * strain is what the keeper names and raises. Each strain asks its brain for
 * a strategy every few seconds; its cells carry the strategy out with
 * physics (run-and-tumble chemotaxis, adhesion, the stir current). */
#ifndef POCKET_DISH_H
#define POCKET_DISH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* the four glasses (docs/BOARDS.md): the same build flags as the tank */
#if defined(TANK_ROUND)
#define DISH_W 466
#define DISH_H 466
#elif defined(TANK_WATCH)
#define DISH_W 410
#define DISH_H 502
#elif defined(TANK_SQUARE)
#define DISH_W 480
#define DISH_H 480
#else
#define DISH_W 448
#define DISH_H 368
#endif
/* the dish is the whole glass, edge to edge: a rectangle with the glass's own
 * round corners (tank.h's geometry). A circle is the same shape with the corner
 * radius half the width, so the pendant's round glass needs no case of its own. */
#if defined(TANK_ROUND)
#define DISH_CORNER_R 233.0f
#elif defined(TANK_WATCH)
#define DISH_CORNER_R 100.0f
#elif defined(TANK_SQUARE)
#define DISH_CORNER_R 40.0f
#else
#define DISH_CORNER_R 18.0f     /* the 1.8's corners are slightly round: things stay clear of them */
#endif
#define DISH_CX (DISH_W * 0.5f)
#define DISH_CY (DISH_H * 0.5f)
#define DISH_RX (DISH_W * 0.5f)   /* half the glass, across and down */
#define DISH_RY (DISH_H * 0.5f)

#define DISH_MAX_CELLS   192    /* ponytail: fixed pool; the device budget is set by the renderer, not RAM */
#define DISH_MAX_STRAINS 6
#define DISH_MAX_PARTS   96
#define FIELD_N          32     /* the nutrient / waste / flow / drug grids are FIELD_N x FIELD_N over the frame */
#define DISH_PET         0      /* strain 0 is always the keeper's */
#define DISH_DIV_TIME    2.6f   /* seconds a cell takes to pinch in two */
#define DISH_WALLS       3      /* walls standing at once */
#define DISH_WALL_PTS    24
#define DISH_WALL_LIFE   20.0f
#define DISH_DRAFT_GENS  2      /* a mutation draft every this many generations: a lazily kept strain makes
                                   ~0.3 a minute, so a card every 6-7 minutes */

/* the first six are the originals (their ids are in saves); the rest arrive as visitors */
typedef enum { SP_COCCUS, SP_BACILLUS, SP_SPIRILLUM, SP_CYANO, SP_MYXO, SP_PHAGE,
               SP_MOLD, SP_VIBRIO, SP_LACTO, SP_HALO, SP_SYMBIONT, SP_CILIATE, SP_DONOR, SP_COUNT } species_t;
extern const bool SPECIES_FRIENDLY[SP_COUNT];   /* a visitor that helps the pet (the toast and the dex say so) */
#define DISH_MUT_N 16                           /* = M_COUNT (dish_mut.h) */
extern const char *const SPECIES_NAMES[SP_COUNT];

/* what a strain decides to do (dish schema v1: the AI's goals). The targeted ones act on another strain. */
typedef enum { ST_FORAGE, ST_CLUSTER, ST_EXPLORE, ST_REST, ST_DIVIDE, ST_SPORE, ST_SEEK_LIGHT, ST_AVOID_LIGHT,
               ST_HUNT, ST_FLEE, ST_GUARD, ST_BEFRIEND, ST_COUNT } strat_id_t;
extern const char *const STRAT_NAMES[ST_COUNT];
#define ST_TARGETED(g) ((g) >= ST_HUNT)

/* a decision: the goal, the strain it is about (-1 none), how urgent, how sure (the AI's softmax) */
typedef struct { uint8_t id; int8_t target; float urgency, confidence; uint8_t say; } strat_t;   /* say: what it says about it (dish_talk.h) */

/* how a cell died (the story's causes) */
enum { DIE_STARVED, DIE_EATEN, DIE_VIRUS, DIE_POISON, DIE_HARSH, DIE_OLD, DIE_COUNT };

/* what a cell inherits (copied with small mutations at every division) */
typedef struct {
    float speed;     /* x the species' base swim speed, 0.5..1.6 */
    float size;      /* x the species' base radius, 0.75..1.35 */
    float hue;       /* 0..1 around the color wheel */
    float heat_opt;  /* the temperature it thrives at, 0..1 (0.5 = room) */
    float salt_tol;  /* 0..1 */
    float drug_res;  /* antibiotic resistance 0..1 */
    float phage_res; /* chance a phage fails to get in, 0..1 */
    float div_rate;  /* x how fast it turns energy into a daughter, 0.6..1.6 */
    float ph_opt;    /* the pH it thrives at, 0 acid .. 1 base (0.5 = neutral) */
} genome_t;

typedef enum { CS_FREE, CS_LIVE, CS_SPORE, CS_DYING } cell_state_t;

/* wires (dish_gene.c): eight senses, eight effects, and the looks a dose paints on a cell */
enum { GS_ALWAYS, GS_LIGHT, GS_FOOD, GS_TOUCH, GS_HURT, GS_KIN, GS_ENERGY, GS_NEAR, GS_COUNT };
enum { GE_SPINE, GE_PHOTO, GE_TETHER, GE_ENGULF, GE_GLOW, GE_THRUST, GE_STICK, GE_TOXIN, GE_COUNT };
enum { LOOK_NONE, LOOK_GLASS, LOOK_SLIME, LOOK_FOAM, LOOK_CHLORO, LOOK_SPINE, LOOK_MAW };
enum { BODY_NONE, BODY_CHAIN, BODY_RING, BODY_STALK };   /* a dose can grow one of these out of a cell */
enum { AW_STILL, AW_HUNGER, AW_QUIET, AW_GUEST, AW_VAULT, AW_FILM };
enum { AE_FEED, AE_SPLIT, AE_SWALLOW };   /* a real event from the catch-up, played back on wake */
#define DISH_GENO_MAX 48
typedef struct { uint8_t sense, effect, gain, thr; } wire_t;
typedef struct { uint8_t used, nwire, look, form; wire_t w[4]; } geno_t;

typedef struct {
    float x, y, vx, vy;
    float ang, spin;         /* heading (rad) and its rate */
    float energy;            /* 0 starve .. 1 ready to divide */
    float health;            /* 0 dead .. 1 */
    float age;               /* seconds */
    float run_t, last_food;  /* run-and-tumble: time left in this run, food sampled at its start */
    float div_t;             /* > 0 while pinching in two (counts up to DIV_TIME) */
    float infect;            /* a phage's payload: at 1 the host bursts */
    float dying_t;           /* the pop animation */
    float phase;             /* animation phase (flagellum, wobble, blink) */
    uint32_t gen;            /* generation (the founders are 0) */
    uint32_t id;             /* unique while the dish runs (the lens follows this, not the slot) */
    float comfort;           /* last tick's comfort, 0..1 (the census reads it rather than computing it again) */
    uint8_t state, species, strain, host;  /* host: a phage clamped onto a cell (index + 1), 0 when free */
    uint8_t nbrs;            /* how many cells touched it last tick (biofilm, quorum)*/
    uint8_t hurt;            /* what hurt it last (DIE_*): the cause if it dies of it */
    uint8_t tether;          /* grapple pili: the foreign cell it holds (index + 1), 0 none */
    float stun_t;            /* > 0: stunned by a zap - it neither moves nor bites */
    float jet_t;             /* > 0: jetting away; < 0: the jet recharging */
    float hatch_t;           /* a spore from a spore burst: seconds until it hatches */
    /* a split, a swallow, a dose: drawn, not saved. show_s of 0 means "draw at full size". */
    float born_t, born_dx, born_dy;   /* > 0: just split or just arrived, separating along (born_dx, born_dy) */
    float pull_x, pull_y;             /* where to draw it, relative to (x, y): the pinch and the swallow */
    float gulp_t, shimmer_t, show_s;
    int16_t twin, gulp_other;         /* the other half of a split, or the other party of a swallow; -1 none */
    uint8_t gulp_role;                /* 0 none, 1 being swallowed, 2 swallowing, 3 closing in */
    uint8_t look;                     /* LOOK_* from the dose, drawn on this cell */
    uint8_t gid;                      /* genotype id (1-based into dish_t.geno), 0 none */
    uint8_t replay;                   /* a scene actor: not part of the colony, not saved */
    uint8_t hold;                     /* a split caught mid-pinch, finished when the wake scene plays it */
    uint8_t form;                     /* BODY_*: the silhouette a dose is growing. not saved */
    float form_t;                     /* 0 just chosen .. 1 grown. the body eases out; it does not pop */
    genome_t g;
} cell_t;

typedef struct {
    bool used;
    uint8_t species;
    char name[12];
    strat_t strat;
    float ask_t;             /* seconds until the brain is asked again */
    /* census, refreshed every tick */
    int count, spores;
    float energy, health, comfort, crowd, filth, infected;
    float cx, cy;            /* centroid of the living cells */
    /* the pet's spore vault: the last healthy genome, and the countdown back from extinction */
    genome_t vault;
    float extinct_t;         /* > 0 while extinct: seconds until respawn */
    uint32_t births, deaths, max_gen;
    /* the build (dish_mut.c): a rank per mutation; only the pet drafts */
    uint8_t mut[DISH_MUT_N];
    float stay_t;            /* a visitor: seconds before it starts to leave (<= 0: it is leaving) */
    /* the mind: the strains its last state line called other1..3 (-1 none), where it is headed, its timers */
    int8_t other[3];
    float wx, wy;            /* an explore waypoint, or where its scouts found food */
    float zap_t, bloom_t;    /* the zap's and the quorum bloom's recharge */
    float decided_t;         /* seconds since its goal last changed (the thought icon's pop-in) */
    uint8_t wire;            /* genotype id the dose gave this strain (0 none); the vault hands it on */
} strain_t;

typedef enum { PT_DROP, PT_RIPPLE, PT_SHARD, PT_SPARK, PT_HEART, PT_ZAP } part_kind_t;   /* PT_ZAP: an arc to (vx, vy) */
typedef struct { float x, y, vx, vy, t, life; uint32_t rgb; uint8_t kind, used; } part_t;

typedef struct {
    cell_t cell[DISH_MAX_CELLS];
    strain_t strain[DISH_MAX_STRAINS];
    part_t part[DISH_MAX_PARTS];
    float nutr[FIELD_N * FIELD_N];     /* food, 0..1 per field cell */
    float waste[FIELD_N * FIELD_N];    /* what the cells leave behind, 0..1 */
    float drug[FIELD_N * FIELD_N];     /* antibiotic, 0..1 (fades) */
    float flowx[FIELD_N * FIELD_N], flowy[FIELD_N * FIELD_N];   /* the stir current, px/s */
    float temp;                        /* the dish's temperature 0..1 (0.5 = room) */
    float ph;                          /* 0 acid .. 1 base (0.5 = neutral) */
    float salt;                        /* 0 fresh .. 1 brine (0.2 = agar) */
    float uv_t;                        /* > 0 while the UV lamp is on: mutations run wild */
    float warm_x, warm_y, warm;        /* a finger held on the glass warms the spot under it */
    bool light;
    float clock;
    float outbreak_t;                  /* seconds until the wild phages may drift in */
    float scare_t;                     /* > 0 after three raps */
    float scare_x, scare_y;
    int n_live;
    uint32_t rng;
    /* the long game (dish_prog.c) */
    uint32_t dna, badges, badges_new, dex, unlocked, births_paid;
    float care_cd;
    uint8_t outbreak_on, was_extinct;
    uint32_t next_id;
    /* the run: one comeback from the vault, then the run is over (dish_prog.c) */
    uint16_t run, run_visitors;
    uint8_t vault_used, run_over, run_muts, crispr;   /* crispr: the pet survived an outbreak and remembers it */
    uint32_t run_dna0;           /* DNA at the run's start (what this run earned) */
    /* the draft (dish_mut.c) */
    uint8_t drafts;              /* waiting to be opened, at most 3 */
    uint8_t offer[3];            /* the dealt cards (0xFF: none dealt) */
    uint32_t next_draft_gen;
    /* the visitors */
    float visit_t, gift_cd;
    /* the keeper's hands: a held finger is light, the box's tilt is gravity */
    float beam_x, beam_y, beam;   /* the light under the finger (0..1, rises while held) */
    float tilt_x, tilt_y;         /* -1..1: which way is down */
    /* the story (dish_story.c): a log of what happened, and a sample of the dish a minute */
    struct { uint16_t min; uint8_t type, a, b, v; uint16_t x; } log[96];
    struct { uint8_t pet, others, food, births, deaths, cause, goal, guests; } series[120];
    uint16_t log_head, log_n, series_head, series_n;
    float run_t;                  /* seconds of this run */
    uint16_t m_births, m_deaths[DIE_COUNT];   /* the minute so far */
    uint16_t best_count;
    /* meetings: which strains touched this tick (a bit per pair), and when a pair may talk again */
    uint64_t met;
    float meet_cd[DISH_MAX_STRAINS][DISH_MAX_STRAINS];
    /* the keeper's walls: drawn with a finger, they stand DISH_WALL_LIFE seconds; the ink refills */
    struct { uint8_t n; float t; float x[DISH_WALL_PTS], y[DISH_WALL_PTS]; } wall[DISH_WALLS];
    float ink;                    /* walls left to draw, 0..DISH_WALLS (fractional: refilling) */
    uint16_t wall_hits, flicks;   /* foreign cells stopped by a wall, flicked away (the quests count them) */
    /* the quest: what the colony asks for now (dish_quest.c) */
    struct { uint8_t kind, state; int8_t who; float t, limit, prog, goal, x, y; uint16_t reward, lost0; } quest;
    float quest_cd;
    /* defend the dish: a wave announced from one side, then fought (dish_quest.c) */
    struct { uint8_t on; int8_t side, strain; float t; uint16_t lost0; } wave;
    float wave_cd;
    uint16_t quests_won, waves_won;
    uint32_t hints;               /* first-time hints already shown (dish_ui.c), saved */
    uint8_t demo;                 /* the dish behind the first-run setup: it lives, but earns and asks nothing (not saved) */
    /* a wire's genotype, shared by every cell that carries the same one (not saved: the verb ranks are) */
    geno_t geno[DISH_GENO_MAX];
    uint8_t geno_n;
    /* what dish_sleep noticed, for the wake-up reveal. not saved. */
    uint8_t away_n, away_kind[5], away_ghosts, away_guest;
    float away_sec, away_gx[8], away_gy[8];
    float wake_cd;                    /* just woke: the colony is on the glass before anyone asks for a chore */
    uint8_t catching;                 /* the catch-up is running: no quests, and the first split waits for the scene */
    uint8_t replaying;                /* the wake scene is on: the main loop holds, the scene ticks its actors */
    uint8_t dose;                     /* DOSE_* the open draft will apply */
    uint8_t dose_form;                /* BODY_* the take in progress will grow */
    uint8_t offer_form[3];            /* the body on each dealt card */
    uint8_t away_ev_n, away_step, away_held;
    uint8_t away_ev_kind[3];
    float away_ev_x[3], away_ev_y[3], away_ev_ang[3];
    float away_step_t;
    int16_t away_actor;
} dish_t;

typedef strat_t (*dish_brain_fn)(const dish_t *d, int strain);

void dish_init(dish_t *d, uint32_t seed, species_t pet, const char *pet_name);
void dish_new_run(dish_t *d, uint32_t seed, species_t pet, const char *pet_name);   /* a fresh dish that keeps DNA, badges, dex, unlocks */
void dish_tick(dish_t *d, float dt, dish_brain_fn brain);
void dish_sleep(dish_t *d, float seconds);              /* time away: a lived catch-up, then the long slow dark */
void dish_away_play(dish_t *d, float dt);               /* play the catch-up's events with the real animations */
void dish_away_cancel(dish_t *d);                       /* leave the scene; a held split is allowed to finish */
int  dish_gulping(const dish_t *d);                     /* a swallow is on screen: nothing else should cover it */

/* strains and cells */
int  dish_add_strain(dish_t *d, species_t sp, const char *name);
int  dish_spawn(dish_t *d, int strain, float x, float y, const genome_t *g);
genome_t dish_species_genome(species_t sp);
bool dish_inside(float x, float y, float margin);     /* within the dish, at least margin px from the glass */
float dish_edge_dist(float x, float y);               /* px from (x,y) to the glass: > 0 inside, < 0 outside */
void dish_edge_normal(float x, float y, float *nx, float *ny);   /* the glass's outward normal nearest (x,y) */
void dish_toward_inside(float *x, float *y, float m); /* (x,y) moved in along the normal until m px from the glass */
float dish_rand(dish_t *d, float lo, float hi);

/* the keeper's hands (frame coordinates) */
void dish_feed(dish_t *d, float x, float y, float amount);   /* a drop of nutrient */
void dish_stir(dish_t *d, float x, float y, float dx, float dy);
void dish_warm(dish_t *d, float x, float y, float dt);       /* a finger held still */
void dish_rap(dish_t *d, float x, float y);                  /* three raps: the cells scatter */
void dish_dose(dish_t *d, float x, float y, float amount);   /* antibiotic */
void dish_antiviral(dish_t *d);                              /* clears the free phages */
void dish_wipe(dish_t *d, float x, float y, float r);        /* wipes waste under a stroke */
void dish_uv(dish_t *d, float seconds);                      /* the UV lamp: more mutations, a little harm */
void dish_light(dish_t *d, float x, float y, float dt);     /* a finger held on the glass is light: food blooms under it */
void dish_set_tilt(dish_t *d, float x, float y);            /* which way is down (-1..1): everything drifts that way */
float dish_light_at(const dish_t *d, float x, float y);      /* the lamp and the finger's light at a point */
void dish_poke(dish_t *d, int cell);                         /* a tap on a germ: it jumps */
void dish_flick(dish_t *d, int cell, float vx, float vy);    /* a flick: the germ is thrown (px/s) */
int  dish_wall_begin(dish_t *d, float x, float y);           /* a new wall (-1: out of ink) */
void dish_wall_add(dish_t *d, int w, float x, float y);      /* the finger moved on: the wall follows */
bool dish_cell_anchor(const dish_t *d, const cell_t *c);     /* a biofilm wall's anchor */
bool dish_cell_scout(const dish_t *d, const cell_t *c);      /* one of the strain's scouts */
int  dish_add_sample(dish_t *d, species_t sp);               /* a new colony mid-dish (-1: no room, or already here) */
int  dish_visit(dish_t *d, species_t sp);                    /* a visitor arrives at the glass's edge (-1: no room, or here) */
bool dish_visitor_here(const dish_t *d, species_t sp);       /* living cells of that kind, not the pet's */
float dish_stress(const dish_t *d, const cell_t *c, float *comfort);   /* harm/s from heat, pH, salt, UV; comfort 0..1 */
void dish_outbreak(dish_t *d, int n);                        /* wild phages drift in from the rim */

/* the save: a compact byte image (~8 KB), append-only. dish_load returns false
 * (and leaves d alone) on anything it doesn't recognise. */
#define DISH_SAVE_MAX 12288
size_t dish_save(const dish_t *d, uint8_t *out, size_t cap);
bool   dish_load(dish_t *d, const uint8_t *in, size_t len, uint32_t seed);

/* fields */
float dish_field(const float *f, float x, float y);          /* bilinear sample */
int   dish_cell_at(const dish_t *d, float x, float y, float slop);   /* the living cell under a point, or -1 */
float dish_cell_radius(const cell_t *c);

/* events (sounds are feedback, never behavior: one listener, may only enqueue) */
enum { DEV_FEED, DEV_DIVIDE, DEV_POP, DEV_BURST, DEV_INFECT, DEV_EXTINCT, DEV_RESPAWN, DEV_OUTBREAK, DEV_STRAT, DEV_BADGE,
       DEV_VISIT, DEV_VISIT_GONE, DEV_GIFT, DEV_DRAFT, DEV_RUN_OVER, DEV_ZAP, DEV_BLOOM, DEV_MEET, DEV_QUEST, DEV_WAVE, DEV_COUNT };
/* DEV_MEET: strain = a * 8 + b, two strains whose cells touched (they talk); DEV_QUEST: strain = the quest's state;
 * DEV_WAVE: strain = 1 announced, 2 begun, 3 over, 0 called off */
/* DEV_BADGE: strain = the badge; DEV_VISIT / DEV_VISIT_GONE: strain = the species; DEV_GIFT: a free card (gene transfer) */
typedef void (*dish_event_fn)(int ev, int strain, void *ud);
void dish_events_set(dish_event_fn fn, void *ud);
void dish_emit(int ev, int strain);

/* the rule brain (dish_brain.c) — the LLM's place once the dish model is distilled */
strat_t dish_brain_rules(const dish_t *d, int strain);
void    dish_encode(const dish_t *d, int strain, char *out, int n);   /* the state line the AI reads (dish schema v1) */
void    dish_fill_others(dish_t *d, int strain);                    /* (re)pick its other1..3: the nearest strains, as the mind sees them */
int     dish_lexicon(const char **out, int cap);                    /* every word the line and the answer use: the tokenizer's vocabulary */

#endif
