//
// Created by luo on 2023/5/29.
//

#ifndef LIBNE_RBTREE_H
#define LIBNE_RBTREE_H

#ifdef cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <assert.h>

#define RBN_COLOR_BLK (0)
#define RBN_COLOR_RED (1)

#define RBT_LEFT(n)  \
  ((n)->children[0])
#define RBT_RIGHT(n) \
  ((n)->children[1])
#define RBT_DIR(n)   \
  (RBT_LEFT((n)->parent) == (n) ? 0 : 1)
#define RBT_REVERSE_DIR(i) \
  (((i) ^ 0x1) & 0x1 )

#define RBT_IS_RED(n) \
  ((n)->color == RBN_COLOR_RED)

typedef struct rbnode {
  char color;
  struct rbnode *parent;
  struct rbnode *children[2];
} rbnode_t;

typedef struct rbtree {
  rbnode_t *root;
  int  (*cmpfn)(rbnode_t *, rbnode_t *);
  void (*rotatefn[2])(rbnode_t **);
} rbtree_t;

static rbnode_t *rbt_min(rbnode_t *n);
static rbnode_t *rbt_next(rbnode_t *n);

static void rbt_rotate_left(rbnode_t **n);
static void rbt_rotate_right(rbnode_t **n);
static void rbt_insert_balance(rbtree_t *tree, rbnode_t *n);

static void rbtree_init(rbtree_t *tree, int(*cmp)(rbnode_t*,rbnode_t*)) {
  tree->root = NULL;
  tree->cmpfn = cmp;
  tree->rotatefn[0] = rbt_rotate_left;
  tree->rotatefn[1] = rbt_rotate_right;
}

static rbnode_t *rbtree_insert(rbtree_t *tree, rbnode_t *n) {
  rbnode_t **tmp;

  RBT_LEFT(n) = RBT_RIGHT(n) = n->parent = NULL;

  tmp = &tree->root;
  while ((*tmp) != NULL) {
    n->parent = *tmp;
    int cmp = tree->cmpfn(n, (*tmp));
    if (cmp < 0)
        tmp = &(RBT_LEFT(*tmp));
    else if (cmp > 0)
        tmp = &(RBT_RIGHT(*tmp));
    else
        return *tmp;
  }

  (*tmp) = n;
  n->color = RBN_COLOR_RED;
  rbt_insert_balance(tree, n);
  return NULL;
}

static void rbtree_move(rbtree_t *dst, rbtree_t *src) {
  dst->root = src->root;
  dst->cmpfn = src->cmpfn;
  dst->rotatefn[0] = rbt_rotate_left;
  dst->rotatefn[1] = rbt_rotate_right;

  src->root = NULL;
}

rbnode_t *rbt_min(rbnode_t *n) {
  if (n == NULL)
    return NULL;
  while (RBT_LEFT(n) != NULL)
    n = RBT_LEFT(n);
  return n;
}

rbnode_t *rbt_next(rbnode_t *n) {
  if (RBT_RIGHT(n) != NULL) {
    n = RBT_RIGHT(n);
    while (RBT_LEFT(n) != NULL)
      n = RBT_LEFT(n);
  } else {
    if (n->parent && n == RBT_LEFT(n->parent))
      return n->parent;
    while (n->parent && n == RBT_RIGHT(n->parent))
      n = n->parent;
    n = n->parent;
  }
  return n;
}

void rbt_rotate_left(rbnode_t **n) {
  rbnode_t *node, *newn;

  node = *n;
  newn = RBT_RIGHT(node);
  *n   = newn;

  if (RBT_LEFT(newn) != NULL)
    RBT_LEFT(newn)->parent = node;

  RBT_RIGHT(node) = RBT_LEFT(newn);
  RBT_LEFT(newn)  = node;
  newn->parent = node->parent;
  node->parent = newn;
}

void rbt_rotate_right(rbnode_t **n) {
  rbnode_t *node, *newn;

  node = *n;
  newn = RBT_LEFT(node);
  *n   = newn;

  if (RBT_RIGHT(newn) != NULL)
    RBT_RIGHT(newn)->parent = node;

  RBT_LEFT(node)  = RBT_RIGHT(newn);
  RBT_RIGHT(newn) = node;
  newn->parent = node->parent;
  node->parent = newn;
}

void rbt_insert_balance(rbtree_t *tree, rbnode_t *node) {
  rbnode_t *uncle, *parent, *grandp;
  int dir;

// 1.红黑树中的节点只能有红色和黑色两种颜色
// 2.根节点一定是黑色
// 3.叶子节点一定是黑色节点（所有空节点都涂为黑色，但在具体实现时不对空节点
//   进行处理，可见下示例图）
// 4.红色节点的子节点只能是黑色节点（从这条准则中我们可以得到一些推论：红色
//   节点的父节点也一定是黑色节点，若父节点是红色节点，则父节点显然不满足定
//   义。从某一节点到子节点的路径中不可能有连续的两个红色节点）
// 5.从任意节点到叶子节点的任意路径上，黑色的节点数量一定相同
  while ((parent = node->parent) != NULL && RBT_IS_RED(parent)) {
    parent->color = RBN_COLOR_BLK;
    dir = RBT_DIR(parent);
    grandp = parent->parent;
    uncle  = grandp->children[RBT_REVERSE_DIR(dir)];
    if (uncle && RBT_IS_RED(uncle)) {
      uncle->color = RBN_COLOR_BLK;
      parent->color = RBN_COLOR_BLK;
      node = parent->parent;
      node->color = RBN_COLOR_RED;
      continue;
    }

    if (dir != RBT_DIR(node)) {
      tree->rotatefn[dir](&(grandp->children[dir]));
      node = parent;
      parent = grandp->children[dir];
    }
    parent->color = RBN_COLOR_BLK;
    grandp->color = RBN_COLOR_RED;
    dir = RBT_REVERSE_DIR(dir);
    if (grandp->parent != NULL) {
      tree->rotatefn[dir](&(grandp->parent->children[RBT_DIR(grandp)]));
    } else
      tree->rotatefn[dir](&tree->root);
  }
  tree->root->color = RBN_COLOR_BLK;
}

#ifdef cpluscplus
}
#endif

#endif //LIBNE_RBTREE_H
