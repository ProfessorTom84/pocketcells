/* dish_gene.h — the first slice of the mutation revamp.
 *
 * A card is a wire (a sense wired to an effect) plus a visible look, dealt
 * into a dose of cells. The card may say what the wire listens for. It does
 * not say what that does. The old 16 verbs stay in dish_mut.c so the existing
 * selftests keep their meanings; a wire fires only when that verb is absent,
 * which is how the equivalence checks prove a composition can stand in for one. */
#ifndef POCKET_DISH_GENE_H
#define POCKET_DISH_GENE_H

#include "dish.h"

enum { DOSE_SPARK, DOSE_SEED, DOSE_FLOOD };

/* true when one of this cell's wires wants `effect` right now */
bool dish_gene_wants(const dish_t *d, const cell_t *c, int effect);
/* give a dose of the pet's cells the wire and the look that stand in for mutation m.
 * returns how many cells received it. */
int  dish_gene_dose(dish_t *d, int mut, int dose);
/* the card's name (no meaning) and the sense it admits to */
void dish_gene_card_name(const dish_t *d, int mut, char *out, int n);
const char *dish_gene_listens(int mut);
const char *dish_gene_body_name(int form);
/* start a swallow. the prey stays alive and is drawn in; the predator pulses.
 * false if either cell cannot. */
bool dish_swallow(dish_t *d, int pred, int prey);

#endif
