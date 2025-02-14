/* { dg-do compile } */
/* { dg-options "-march=rv64gcv -mabi=lp64d -O3 -ftree-vectorize --param riscv-autovec-lmul=dynamic -fdump-tree-vect-details" } */

#include <stdint-gcc.h>
#include <stddef.h>

void
foo (size_t *__restrict a, size_t *__restrict b, int n)
{
  for (int i = 0; i < n; i++)
    a[i] = a[i] + b[i];
}

/* { dg-final { scan-assembler {e64,m8} } } */
/* { dg-final { scan-assembler-not {csrr} } } */
/* { dg-final { scan-tree-dump "Maximum lmul = 8" "vect" } } */
/* { dg-final { scan-tree-dump-not "Maximum lmul = 4" "vect" } } */
/* { dg-final { scan-tree-dump-not "Maximum lmul = 2" "vect" } } */
/* { dg-final { scan-tree-dump-not "Maximum lmul = 1" "vect" } } */
