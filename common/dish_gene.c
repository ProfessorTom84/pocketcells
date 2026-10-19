/* dish_gene.c — see dish_gene.h. No I/O: sim, wasm and firmware share this. */
#include "dish_gene.h"
#include "dish_mut.h"
#include <stdio.h>
#include <string.h>

static const char *const LEFT[] = {
    "VELLUM", "ORRIN", "PALE", "MOTH", "SABLE", "BRINE", "LUMEN", "NELL", "QUILL", "KIN", "WEFT", "HUSH"
};
static const char *const RIGHT[] = {
    "ORRIN", "WEFT", "SALT", "MOTE", "GLASS", "PIN", "VEIN", "CORD", "ASH", "LOOM", "DRIFT", "MARL"
};
static const char *const LISTENS[M_COUNT] = {
    [M_SPIKES] = "CONTACT", [M_TOXIN] = "NEIGHBORS", [M_ZAP] = "A STRANGER NEAR", [M_JET] = "BEING HURT",
    [M_ENGULF] = "A NEIGHBOR", [M_SWARM] = "KIN NEARBY", [M_BIOFILM] = "KIN TOUCHING", [M_PHOTO] = "LIGHT",
    [M_LURE] = "ITS ENERGY", [M_SPOREBURST] = "ITS HEALTH", [M_TAMER] = "A VIRUS", [M_MIMIC] = "THE DISH",
    [M_SCOUTS] = "WHERE FOOD IS", [M_GRAPPLE] = "A TOUCH", [M_SYMBIOSIS] = "A FRIEND", [M_QUORUM] = "A CROWD",
};
/* sense, effect, look — one composition per old verb, eight effects in all */
static const uint8_t MAP_S[M_COUNT] = {
    GS_ALWAYS, GS_NEAR, GS_NEAR, GS_HURT, GS_NEAR, GS_KIN, GS_KIN, GS_LIGHT,
    GS_ENERGY, GS_HURT, GS_NEAR, GS_ALWAYS, GS_FOOD, GS_TOUCH, GS_NEAR, GS_KIN
};
static const uint8_t MAP_E[M_COUNT] = {
    GE_SPINE, GE_TOXIN, GE_GLOW, GE_THRUST, GE_ENGULF, GE_STICK, GE_STICK, GE_PHOTO,
    GE_GLOW, GE_THRUST, GE_TOXIN, GE_GLOW, GE_THRUST, GE_TETHER, GE_STICK, GE_THRUST
};
static const uint8_t MAP_L[M_COUNT] = {
    LOOK_SPINE, LOOK_SLIME, LOOK_GLASS, LOOK_FOAM, LOOK_MAW, LOOK_SLIME, LOOK_SLIME, LOOK_CHLORO,
    LOOK_GLASS, LOOK_FOAM, LOOK_GLASS, LOOK_GLASS, LOOK_FOAM, LOOK_GLASS, LOOK_SLIME, LOOK_CHLORO
};

static float sense_of(const dish_t *d, const cell_t *c, int sense) {
    switch (sense) {
    case GS_ALWAYS: return 1;
    case GS_LIGHT:  return dish_light_at(d, c->x, c->y);
    case GS_FOOD:   return dish_field(d->nutr, c->x, c->y);
    case GS_TOUCH:  return c->nbrs ? 1 : 0;
    case GS_HURT:   return 1 - c->health;
    case GS_KIN:    return c->nbrs ? 0.8f : 0;
    case GS_ENERGY: return c->energy;
    case GS_NEAR:   return c->nbrs ? 1 : 0;
    default:        return 0;
    }
}

bool dish_gene_wants(const dish_t *d, const cell_t *c, int effect) {
    if (!c->gid || c->gid > d->geno_n || c->gid > DISH_GENO_MAX) return false;
    const geno_t *g = &d->geno[c->gid - 1];
    for (int i = 0; i < g->nwire && i < 4; i++) {
        if (g->w[i].effect != effect) continue;
        float thr = g->w[i].thr / 255.0f;
        if (sense_of(d, c, g->w[i].sense) + 1e-4f >= thr) return true;
    }
    return false;
}

static int intern_gene(dish_t *d, int mut) {
    if (mut < 0 || mut >= M_COUNT) return 0;
    uint8_t sense = MAP_S[mut], effect = MAP_E[mut], look = MAP_L[mut];
    for (int i = 0; i < d->geno_n; i++) {
        const geno_t *g = &d->geno[i];
        if (g->nwire == 1 && g->look == look && g->w[0].sense == sense && g->w[0].effect == effect) return i + 1;
    }
    if (d->geno_n >= DISH_GENO_MAX) return 0;
    geno_t *g = &d->geno[d->geno_n];
    memset(g, 0, sizeof *g);
    g->used = 1; g->nwire = 1; g->look = look;
    g->w[0].sense = sense; g->w[0].effect = effect; g->w[0].gain = 180; g->w[0].thr = 30;
    d->geno_n++;
    return d->geno_n;
}

int dish_gene_dose(dish_t *d, int mut, int dose) {
    int gid = intern_gene(d, mut);
    if (!gid) return 0;
    int live[DISH_MAX_CELLS], n = 0;
    for (int i = 0; i < DISH_MAX_CELLS; i++) {
        cell_t *c = &d->cell[i];
        if (c->state == CS_LIVE && c->strain == DISH_PET && !c->gulp_role) live[n++] = i;
    }
    if (!n) return 0;
    int want = dose == DOSE_SPARK ? 2 : dose == DOSE_FLOOD ? (n + 1) / 2 : (n * 15 + 99) / 100;
    if (want < 1) want = 1;
    if (want > n) want = n;
    /* shuffle with the dish's own stream so a dose is a real sample, not the first slots */
    for (int i = n - 1; i > 0; i--) {
        int j = (int)dish_rand(d, 0, (float)i + 0.999f);
        if (j < 0) j = 0;
        if (j > i) j = i;
        int t = live[i]; live[i] = live[j]; live[j] = t;
    }
    uint8_t look = d->geno[gid - 1].look;
    for (int k = 0; k < want; k++) {
        cell_t *c = &d->cell[live[k]];
        c->gid = (uint8_t)gid; c->look = look; c->shimmer_t = 1.7f;
    }
    return want;
}

void dish_gene_card_name(const dish_t *d, int mut, char *out, int n) {
    uint32_t h = (uint32_t)mut * 131u + (uint32_t)d->run * 17u + 11u;
    snprintf(out, (size_t)n, "%s-%s", LEFT[h % 12], RIGHT[(h / 12) % 12]);
}
const char *dish_gene_listens(int mut) {
    if (mut < 0 || mut >= M_COUNT) return "SOMETHING";
    return LISTENS[mut];
}

bool dish_swallow(dish_t *d, int pred, int prey) {
    if (!d || pred < 0 || prey < 0 || pred == prey || pred >= DISH_MAX_CELLS || prey >= DISH_MAX_CELLS) return false;
    cell_t *p = &d->cell[pred], *o = &d->cell[prey];
    if (p->state != CS_LIVE || o->state != CS_LIVE || p->gulp_role || o->gulp_role) return false;
    if (p->species == SP_PHAGE || o->species == SP_PHAGE) return false;
    p->gulp_role = 2; o->gulp_role = 1;
    p->gulp_other = (int16_t)prey; o->gulp_other = (int16_t)pred;
    p->gulp_t = o->gulp_t = 0.82f;
    p->show_s = 1;
    return true;
}
