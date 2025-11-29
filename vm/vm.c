/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "threads/mmu.h"

static struct list frame_table;
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *new_page = (struct page *)malloc(sizeof(struct page));
		if (new_page == NULL)
		{
			goto err;
		}
		bool (*page_initializer)(struct page *, enum vm_type, void *);
		if (VM_TYPE(type) & VM_ANON)
		{
			page_initializer = anon_initializer;
		}
		else if (VM_TYPE(type) & VM_FILE)
		{
			page_initializer = file_backed_initializer;
		}
		else
		{
			goto err;
		}
		uninit_new(new_page, upage, init, type, aux, page_initializer);
		new_page->writable = writable;
		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, new_page))
		{
			free(new_page);
			goto err;
		}
	}
	return true;
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
	struct page *page;
	/* TODO: Fill this function. */
	struct page *temp = malloc(sizeof(struct page));
	temp->va = pg_round_down(va);

	struct hash_elem *e = hash_find(&spt->spt_hash, &temp->hash_elem);
	if (e == NULL)
	{
		free(temp);
		return NULL;
	}
	page = hash_entry(e, struct page, hash_elem);
	free(temp);
	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page)
{
	int succ = false;
	/* TODO: Fill this function. */
	succ = (hash_insert(&spt->spt_hash, &page->hash_elem) == NULL);
	return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	hash_delete(&spt->spt_hash, &page->hash_elem);
	vm_dealloc_page(page);
	// return true; // void인데 왜 리턴?
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	// 프레임 테이블이 필요
	// LRU??

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */
	if (victim == NULL)
	{
		return NULL;
	}
	struct page *page = victim->page;
	if (page != NULL)
	{
		if (!swap_out(page))
		{
			return NULL;
		}
		page->frame = NULL;
	}
	victim->page = NULL;
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	void *new_kva = palloc_get_page(PAL_USER | PAL_ZERO); // 0으로 초기화 해야되나??
	if (new_kva != NULL)
	{
		frame = (struct frame *)malloc(sizeof(struct frame));
		if (frame == NULL)
		{
			return NULL;
		}
		frame->kva = new_kva;
		frame->page = NULL;
		// list_push_back(&frame_table, &frame->list_elem);
	}
	else
	{
		frame = vm_evict_frame();
	}
	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	void *va = pg_round_down(addr);
	struct page *page = spt_find_page(spt, va);
	if (!page)
	{
		return false;
	}
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va)
{
	struct page *page = spt_find_page(&thread_current()->spt, va);
	/* TODO: Fill this function */
	if (page == NULL)
	{
		return false;
	}
	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable))
	{
		return false;
	}
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt)
{
	hash_init(&spt->spt_hash, hash_hash, hash_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

uint64_t hash_hash(const struct hash_elem *e, void *aux UNUSED)
{
	struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
	struct page *pa = hash_entry(a, struct page, hash_elem);
	struct page *pb = hash_entry(b, struct page, hash_elem);
	return pa->va < pb->va;
}
