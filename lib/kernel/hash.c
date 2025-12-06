/* Hash table.

   This data structure is thoroughly documented in the Tour of
   Pintos for Project 3.

   See hash.h for basic information. */

#include "hash.h"
#include "../debug.h"
#include "threads/malloc.h"

#define list_elem_to_hash_elem(LIST_ELEM) \
	list_entry(LIST_ELEM, struct hash_elem, list_elem)

static struct list *find_bucket(struct hash *, struct hash_elem *);
static struct hash_elem *find_elem(struct hash *, struct list *,
								   struct hash_elem *);
static void insert_elem(struct hash *, struct list *, struct hash_elem *);
static void remove_elem(struct hash *, struct hash_elem *);
static void rehash(struct hash *);

/* 해시 테이블 H를 초기화하는데, HASH를 사용해 해시 값을 계산하고 LESS를 사용해 해시 요소들을 비교하며, 
보조 데이터 AUX가 주어집니다.*/
bool hash_init(struct hash *h,
			   hash_hash_func *hash, hash_less_func *less, void *aux)
{
	h->elem_cnt = 0;
	h->bucket_cnt = 4;
	h->buckets = malloc(sizeof *h->buckets * h->bucket_cnt);
	h->hash = hash;
	h->less = less;
	h->aux = aux;

	if (h->buckets != NULL)
	{
		hash_clear(h, NULL);
		return true;
	}
	else
		return false;
}

/* “H에서 모든 요소들을 제거한다.
만약 DESTRUCTOR가 null이 아니라면, 해시 안의 각 요소마다 그것이 호출된다.
DESTRUCTOR는, 적절하다면, 그 해시 요소가 사용한 메모리를 해제(deallocate)할 수 있다.
그러나 hash_clear()가 실행되는 동안,
hash_clear(), hash_destroy(), hash_insert(), hash_replace(), hash_delete() 같은 함수들을 사용하여
해시 테이블 H를 수정하는 것은
DESTRUCTOR 안에서 이루어지든, 다른 곳에서 이루어지든, undefined behavior(정의되지 않은 동작)를 일으킨다. */
void hash_clear(struct hash *h, hash_action_func *destructor)
{
	size_t i;

	for (i = 0; i < h->bucket_cnt; i++)
	{
		struct list *bucket = &h->buckets[i];

		if (destructor != NULL)
			while (!list_empty(bucket))
			{
				struct list_elem *list_elem = list_pop_front(bucket);
				struct hash_elem *hash_elem = list_elem_to_hash_elem(list_elem);
				destructor(hash_elem, h->aux);
			}

		list_init(bucket);
	}

	h->elem_cnt = 0;
}

/* “해시 테이블 H를 파괴(destroy)한다.
만약 DESTRUCTOR가 null이 아니라면, 먼저 해시 안의 각 요소에 대해 호출된다.
DESTRUCTOR는, 적절하다면, 그 해시 요소가 사용한 메모리를 해제(deallocate)할 수 있다.
하지만 hash_clear()가 실행되는 동안,
hash_clear(), hash_destroy(), hash_insert(), hash_replace(), hash_delete() 같은 함수로
해시 테이블 H를 수정하는 것은,
DESTRUCTOR 안에서든 그 외의 곳에서든, undefined behavior(동작이 정의되지 않음)을 초래한다.” */
void hash_destroy(struct hash *h, hash_action_func *destructor)
{
	if (destructor != NULL)
		hash_clear(h, destructor);
	free(h->buckets);
}

/* NEW를 해시 테이블 H에 삽입하고,
그와 동일한 요소가 테이블 안에 이미 없다면 null 포인터를 반환한다.
만약 동일한 요소가 이미 테이블 안에 있다면, NEW를 삽입하지 않고
그 동일한 요소를 반환한다. */
struct hash_elem *
hash_insert(struct hash *h, struct hash_elem *new)
{
	struct list *bucket = find_bucket(h, new);
	struct hash_elem *old = find_elem(h, bucket, new);

	if (old == NULL)
		insert_elem(h, bucket, new);

	rehash(h);

	return old;
}

/* NEW를 해시 테이블 H에 삽입하며,
이미 테이블 안에 동일한 요소가 있다면 그것을 대체하고,
그 동일한 요소를 반환한다. */
struct hash_elem *
hash_replace(struct hash *h, struct hash_elem *new)
{
	struct list *bucket = find_bucket(h, new);
	struct hash_elem *old = find_elem(h, bucket, new);

	if (old != NULL)
		remove_elem(h, old);
	insert_elem(h, bucket, new);

	rehash(h);

	return old;
}

/* 해시 테이블 H에서 E와 동일한 요소를 찾아서 반환하며,
동일한 요소가 테이블에 존재하지 않으면 null 포인터를 반환한다. */
struct hash_elem *
hash_find(struct hash *h, struct hash_elem *e)
{
	return find_elem(h, find_bucket(h, e), e);
}

/* 해시 테이블 H에서 E와 동일한 요소를 찾아 제거하고 반환한다.
테이블에 동일한 요소가 존재하지 않았다면 null 포인터를 반환한다.

해시 테이블의 요소들이 동적으로 할당되어 있거나,
동적으로 할당된 자원을 소유하고 있다면,
그것들을 해제하는 책임은 호출자에게 있다. */
struct hash_elem *
hash_delete(struct hash *h, struct hash_elem *e)
{
	struct hash_elem *found = find_elem(h, find_bucket(h, e), e);
	if (found != NULL)
	{
		remove_elem(h, found);
		rehash(h);
	}
	return found;
}

/* 해시 테이블 H 안의 각 요소에 대해, 임의의 순서로 ACTION을 호출한다.
hash_apply()가 실행 중일 때,
hash_clear(), hash_destroy(), hash_insert(), hash_replace(), hash_delete() 같은 함수들을 사용해
해시 테이블 H를 수정하는 것은,
그것이 ACTION 안에서 수행되든 다른 곳에서 수행되든,
undefined behavior(정의되지 않은 동작)를 일으킨다. */
void hash_apply(struct hash *h, hash_action_func *action)
{
	size_t i;

	ASSERT(action != NULL);

	for (i = 0; i < h->bucket_cnt; i++)
	{
		struct list *bucket = &h->buckets[i];
		struct list_elem *elem, *next;

		for (elem = list_begin(bucket); elem != list_end(bucket); elem = next)
		{
			next = list_next(elem);
			action(list_elem_to_hash_elem(elem), h->aux);
		}
	}
}

/* 해시 테이블 H를 순회(iterate)하기 위해 I를 초기화한다.

   	순회 예시:

   	struct hash_iterator i;

	hash_first (&i, h);
	while (hash_next (&i))
	{
  		struct foo *f = hash_entry (hash_cur (&i), struct foo, elem);
  		...f로 무언가를 수행...
	}

   	해시 테이블 H를 순회하는 동안,
	hash_clear(), hash_destroy(), hash_insert(), hash_replace(), hash_delete()와 같은 함수들을 사용해
	해시 테이블 H를 수정하면, 모든 이터레이터가 무효화된다. */
void hash_first(struct hash_iterator *i, struct hash *h)
{
	ASSERT(i != NULL);
	ASSERT(h != NULL);

	i->hash = h;
	i->bucket = i->hash->buckets;
	i->elem = list_elem_to_hash_elem(list_head(i->bucket));
}

/* 현재 iterator가 가리키는 위치에서 “다음 요소”를 찾아서 iterator를 그쪽으로 이동시키고, 
그 요소의 포인터를 반환하는 함수.

순회(iteration) 중에 hash_clear(), hash_destroy(), hash_insert(),
hash_replace(), hash_delete() 같은 함수로
해시 테이블 H를 수정하면 모든 이터레이터가 무효화된다. */
struct hash_elem *
hash_next(struct hash_iterator *i)
{
	ASSERT(i != NULL);

	i->elem = list_elem_to_hash_elem(list_next(&i->elem->list_elem));
	while (i->elem == list_elem_to_hash_elem(list_end(i->bucket)))
	{
		if (++i->bucket >= i->hash->buckets + i->hash->bucket_cnt)
		{
			i->elem = NULL;
			break;
		}
		i->elem = list_elem_to_hash_elem(list_begin(i->bucket));
	}

	return i->elem;
}

/* 해시 테이블 순회(iteration)에서 현재 요소를 반환하며,
테이블의 끝이라면 null 포인터를 반환한다.
hash_first()를 호출한 뒤 hash_next()를 호출하기 전까지는
이 함수를 호출하면 undefined behavior(정의되지 않은 동작)이다. */
struct hash_elem *
hash_cur(struct hash_iterator *i)
{
	return i->elem;
}

/* Returns the number of elements in H. */
size_t
hash_size(struct hash *h)
{
	return h->elem_cnt;
}

/* Returns true if H contains no elements, false otherwise. */
bool hash_empty(struct hash *h)
{
	return h->elem_cnt == 0;
}

/* Fowler-Noll-Vo hash constants, for 32-bit word sizes. */
#define FNV_64_PRIME 0x00000100000001B3UL
#define FNV_64_BASIS 0xcbf29ce484222325UL

/* Returns a hash of the SIZE bytes in BUF. */
uint64_t
hash_bytes(const void *buf_, size_t size)
{
	/* Fowler-Noll-Vo 32-bit hash, for bytes. */
	const unsigned char *buf = buf_;
	uint64_t hash;

	ASSERT(buf != NULL);

	hash = FNV_64_BASIS;
	while (size-- > 0)
		hash = (hash * FNV_64_PRIME) ^ *buf++;

	return hash;
}

/* Returns a hash of string S. */
uint64_t
hash_string(const char *s_)
{
	const unsigned char *s = (const unsigned char *)s_;
	uint64_t hash;

	ASSERT(s != NULL);

	hash = FNV_64_BASIS;
	while (*s != '\0')
		hash = (hash * FNV_64_PRIME) ^ *s++;

	return hash;
}

/* Returns a hash of integer I. */
uint64_t
hash_int(int i)
{
	return hash_bytes(&i, sizeof i);
}

/* Returns the bucket in H that E belongs in. */
static struct list *
find_bucket(struct hash *h, struct hash_elem *e)
{
	size_t bucket_idx = h->hash(e, h->aux) & (h->bucket_cnt - 1);
	return &h->buckets[bucket_idx];
}

/* H 안의 BUCKET에서 E와 동일한 해시 요소를 탐색한다.
찾으면 그 요소를 반환하고,
그렇지 않으면 null 포인터를 반환한다. */
static struct hash_elem *
find_elem(struct hash *h, struct list *bucket, struct hash_elem *e)
{
	struct list_elem *i;

	for (i = list_begin(bucket); i != list_end(bucket); i = list_next(i))
	{
		struct hash_elem *hi = list_elem_to_hash_elem(i);
		if (!h->less(hi, e, h->aux) && !h->less(e, hi, h->aux))
			return hi;
	}
	return NULL;
}

/* Returns X with its lowest-order bit set to 1 turned off. */
static inline size_t
turn_off_least_1bit(size_t x)
{
	return x & (x - 1);
}

/* Returns true if X is a power of 2, otherwise false. */
static inline size_t
is_power_of_2(size_t x)
{
	return x != 0 && turn_off_least_1bit(x) == 0;
}

/* Element per bucket ratios. */
#define MIN_ELEMS_PER_BUCKET 1	/* Elems/bucket < 1: reduce # of buckets. */
#define BEST_ELEMS_PER_BUCKET 2 /* Ideal elems/bucket. */
#define MAX_ELEMS_PER_BUCKET 4	/* Elems/bucket > 4: increase # of buckets. */

/* 해시 테이블 H의 버킷 수를 이상적인 값에 맞도록 변경한다.
이 함수는 메모리 부족(out-of-memory) 상태 때문에 실패할 수 있지만,
그럴 경우 해시 접근이 단지 덜 효율적일 뿐이며,
계속해서 동작을 이어갈 수는 있다. */
static void
rehash(struct hash *h)
{
	size_t old_bucket_cnt, new_bucket_cnt;
	struct list *new_buckets, *old_buckets;
	size_t i;

	ASSERT(h != NULL);

	/* Save old bucket info for later use. */
	old_buckets = h->buckets;
	old_bucket_cnt = h->bucket_cnt;

	/* Calculate the number of buckets to use now.
	   We want one bucket for about every BEST_ELEMS_PER_BUCKET.
	   We must have at least four buckets, and the number of
	   buckets must be a power of 2. */
	new_bucket_cnt = h->elem_cnt / BEST_ELEMS_PER_BUCKET;
	if (new_bucket_cnt < 4)
		new_bucket_cnt = 4;
	while (!is_power_of_2(new_bucket_cnt))
		new_bucket_cnt = turn_off_least_1bit(new_bucket_cnt);

	/* Don't do anything if the bucket count wouldn't change. */
	if (new_bucket_cnt == old_bucket_cnt)
		return;

	/* Allocate new buckets and initialize them as empty. */
	new_buckets = malloc(sizeof *new_buckets * new_bucket_cnt);
	if (new_buckets == NULL)
	{
		/* Allocation failed.  This means that use of the hash table will
		   be less efficient.  However, it is still usable, so
		   there's no reason for it to be an error. */
		return;
	}
	for (i = 0; i < new_bucket_cnt; i++)
		list_init(&new_buckets[i]);

	/* Install new bucket info. */
	h->buckets = new_buckets;
	h->bucket_cnt = new_bucket_cnt;

	/* Move each old element into the appropriate new bucket. */
	for (i = 0; i < old_bucket_cnt; i++)
	{
		struct list *old_bucket;
		struct list_elem *elem, *next;

		old_bucket = &old_buckets[i];
		for (elem = list_begin(old_bucket);
			 elem != list_end(old_bucket); elem = next)
		{
			struct list *new_bucket = find_bucket(h, list_elem_to_hash_elem(elem));
			next = list_next(elem);
			list_remove(elem);
			list_push_front(new_bucket, elem);
		}
	}

	free(old_buckets);
}

/* Inserts E into BUCKET (in hash table H). */
static void
insert_elem(struct hash *h, struct list *bucket, struct hash_elem *e)
{
	h->elem_cnt++;
	list_push_front(bucket, &e->list_elem);
}

/* Removes E from hash table H. */
static void
remove_elem(struct hash *h, struct hash_elem *e)
{
	h->elem_cnt--;
	list_remove(&e->list_elem);
}
