#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* Hash table.
 *
 * This data structure is thoroughly documented in the Tour of
 * Pintos for Project 3.
 *
 * This is a standard hash table with chaining.  To locate an
 * element in the table, we compute a hash function over the
 * element's data and use that as an index into an array of
 * doubly linked lists, then linearly search the list.
 *
 * The chain lists do not use dynamic allocation.  Instead, each
 * structure that can potentially be in a hash must embed a
 * struct hash_elem member.  All of the hash functions operate on
 * these `struct hash_elem's.  The hash_entry macro allows
 * conversion from a struct hash_elem back to a structure object
 * that contains it.  This is the same technique used in the
 * linked list implementation.  Refer to lib/kernel/list.h for a
 * detailed explanation. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Hash element. */
struct hash_elem
{
	struct list_elem list_elem;
};

/* 포인터 HASH_ELEM을, 그 HASH_ELEM이 포함(내장)되어 있는 바깥 구조체(STRUCT 타입)의 포인터로 변환한다.
STRUCT는 바깥 구조체의 이름을, MEMBER는 그 구조체 안에서 hash element가 들어 있는 멤버의 이름을 지정한다.
사용 예시는 이 파일 맨 위에 있는 큰 주석을 참고하라. -ex) (e,struct page,h_elem) */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER) \
	((STRUCT *)((uint8_t *)&(HASH_ELEM)->list_elem - offsetof(STRUCT, MEMBER.list_elem)))

/* Computes and returns the hash value for hash element E, given
 * auxiliary data AUX. */
typedef uint64_t hash_hash_func(const struct hash_elem *e, void *aux);
//-> 요소에서 키 뽑아서 해시값 계산해라

/* Compares the value of two hash elements A and B, given
 * auxiliary data AUX.  Returns true if A is less than B, or
 * false if A is greater than or equal to B. */
typedef bool hash_less_func(const struct hash_elem *a,
							const struct hash_elem *b,
							void *aux);
//-> 요소 둘 비교해서 정렬 기준 제공해라

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
typedef void hash_action_func(struct hash_elem *e, void *aux);
//-> hash 각 요소에 적용할 action함수
/* Hash table. */
struct hash
{
	size_t elem_cnt;	  /* Number of elements in table. */
	size_t bucket_cnt;	  /* Number of buckets, a power of 2. */
	struct list *buckets; /* Array of `bucket_cnt' lists. */
	hash_hash_func *hash; /* Hash function. */
	hash_less_func *less; /* Comparison function. */
	void *aux;			  /* Auxiliary data for `hash' and `less'. */
};

/* A hash table iterator. */
struct hash_iterator
{
	struct hash *hash;		/* The hash table. */
	struct list *bucket;	/* Current bucket. */
	struct hash_elem *elem; /* Current hash element in current bucket. */
};

/* Basic life cycle. */
bool hash_init(struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear(struct hash *, hash_action_func *);
void hash_destroy(struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert(struct hash *, struct hash_elem *); //
struct hash_elem *hash_replace(struct hash *, struct hash_elem *);
struct hash_elem *hash_find(struct hash *, struct hash_elem *); //ht,elem->해당 elem 존재하는지 확인
struct hash_elem *hash_delete(struct hash *, struct hash_elem *);

/* Iteration. */
void hash_apply(struct hash *, hash_action_func *); //해시 테이블 H 안의 각 요소에 대해, 임의의 순서로 ACTION을 호출
void hash_first(struct hash_iterator *, struct hash *); //해시 테이블 H를 순회(iterate)하기 위해 I를 초기화
struct hash_elem *hash_next(struct hash_iterator *); //현재 iterator가 가리키는 위치에서 “다음 요소”를 찾아서 iterator를 그쪽으로 이동시키고 포인터 반환
struct hash_elem *hash_cur(struct hash_iterator *); //해시 테이블 순회(iteration)에서 현재 요소를 반환

/* Information. */
size_t hash_size(struct hash *);
bool hash_empty(struct hash *);

/* Sample hash functions. */
uint64_t hash_bytes(const void *, size_t);
uint64_t hash_string(const char *);
uint64_t hash_int(int);

#endif /* lib/kernel/hash.h */
