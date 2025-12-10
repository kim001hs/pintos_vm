/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"
#include "threads/mmu.h"
#include "devices/timer.h"
#include <string.h>
#include <stdio.h>
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "filesys/file.h"
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
	/* TODO: Your code goes here. */;
	list_init(&frame_table);
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
		new_page->last_used_tick = timer_ticks();
		new_page->pml4 = thread_current()->pml4;
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
	struct page temp;
	temp.va = pg_round_down(va);

	struct hash_elem *e = hash_find(&spt->spt_hash, &temp.hash_elem);
	if (e == NULL)
	{
		return NULL;
	}
	page = hash_entry(e, struct page, hash_elem);
	page->last_used_tick = timer_ticks();
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

	if (!list_empty(&frame_table))
	{
		// LRU: find frame with minimum last_used_tick that has a page
		struct list_elem *e;
		uint64_t min_tick = UINT64_MAX;
		for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e))
		{
			struct frame *f = list_entry(e, struct frame, frame_elem);
			if (f->page != NULL && f->ref_count == 1)
			{
				uint64_t tick = f->page->last_used_tick;
				if (tick < min_tick)
				{
					min_tick = tick;
					victim = f;
				}
			}
		}
	}
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
			palloc_free_page(new_kva);
			return NULL;
		}
		frame->kva = new_kva;
		frame->page = NULL;
		frame->ref_count = 1;
		list_push_back(&frame_table, &frame->frame_elem);
	}
	else
	{
		frame = vm_evict_frame();
	}
	if (frame == NULL)
		PANIC("vm_get_frame: failed to get frame");
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr)
{
	if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true))
	{
		vm_claim_page(addr);
	}
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page)
{
	if (page == NULL)
	{
		return false;
	}
	struct frame *old_frame = page->frame;

	// ref_count가 1이면 이 페이지를 사용하는 프로세스가 하나뿐이므로
	// 새 프레임을 할당할 필요 없이 쓰기 권한만 복원
	if (old_frame->ref_count == 1)
	{
		return pml4_set_page(page->pml4, page->va, old_frame->kva, page->writable);
	}

	// ref_count가 2 이상이면 실제로 페이지를 복사
	struct frame *new_frame = vm_get_frame();
	new_frame->page = page;
	page->frame = new_frame;

	memcpy(new_frame->kva, old_frame->kva, PGSIZE);
	old_frame->ref_count--;

	return pml4_set_page(page->pml4, page->va, page->frame->kva, page->writable);
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present)
{
	struct supplemental_page_table *spt = &thread_current()->spt;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	void *va = pg_round_down(addr);
	struct page *page = spt_find_page(spt, va);
	if (!page)
	{
		void *rsp = f->rsp;
		if (!user)
		{
			rsp = thread_current()->rsp;
		}
		if (addr < (USER_STACK - (1 << 20)) || addr >= USER_STACK || addr < rsp - 8)
		{
			return false;
		}
		vm_stack_growth(va);
		page = spt_find_page(spt, va);
		if (!page)
		{
			return false;
		}
	}
	if (write && !page->writable)
	{
		return false;
	}
	// COW: write fault on a read-only page with a frame (shared)
	if (write && !not_present && page->frame != NULL && page->frame->ref_count > 1)
	{
		return vm_handle_wp(page);
	}
	// COW: page is marked read-only temporarily for COW, but actually writable
	if (write && !not_present && page->frame != NULL && page->frame->ref_count == 1 && page->writable)
	{
		return vm_handle_wp(page);
	}
	bool result = vm_do_claim_page(page);
	if (result)
	{
		page->last_used_tick = timer_ticks();
	}
	return result;
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
	return swap_in(page, frame->kva); // lazy_loading
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt)
{
	hash_init(&spt->spt_hash, hash_hash, hash_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst, struct supplemental_page_table *src)
{
	// todo: swap out상태라면 swap in 시켜서 사용해야함.
	struct hash_iterator temp;
	hash_first(&temp, &src->spt_hash);
	while (hash_next(&temp))
	{
		struct page *src_page = hash_entry(hash_cur(&temp), struct page, hash_elem);
		enum vm_type type = VM_TYPE(src_page->operations->type);

		if (type == VM_UNINIT)
		{
			// UNINIT 페이지는 자식에게도 UNINIT으로 복사
			// aux 데이터 복사 (file_reopen 필요)
			struct new_aux *src_aux = (struct new_aux *)src_page->uninit.aux;
			struct new_aux *new_aux = (struct new_aux *)malloc(sizeof(struct new_aux));
			if (new_aux == NULL)
			{
				return false;
			}
			struct file *reopened_file = file_reopen(src_aux->file);
			if (reopened_file == NULL)
			{
				free(new_aux);
				return false;
			}
			new_aux->file = reopened_file;
			new_aux->offset = src_aux->offset;
			new_aux->page_read_bytes = src_aux->page_read_bytes;

			// UNINIT 페이지 생성
			if (!vm_alloc_page_with_initializer(src_page->uninit.type, src_page->va,
												src_page->writable, src_page->uninit.init, new_aux))
			{
				file_close(reopened_file);
				free(new_aux);
				return false;
			}
		}
		else if (type == VM_ANON || type == VM_FILE)
		{
			// 실제 materialized된 페이지만 여기서 처리
			if (src_page->frame == NULL)
			{
				// frame이 없으면 UNINIT이거나 swap out된 페이지
				// 이 경우는 위의 UNINIT 처리에서 다뤄야 함
				continue;
			}
			// 그냥 부모 페이지를 복사
			struct page *dst_page = (struct page *)malloc(sizeof(struct page));
			if (!dst_page)
			{
				return false;
			}
			memcpy(dst_page, src_page, sizeof(struct page));
			dst_page->pml4 = thread_current()->pml4;
			if (type == VM_FILE)
			{
				dst_page->file.file = file_reopen(src_page->file.file);
			}
			src_page->frame->ref_count++;
			if (!spt_insert_page(dst, dst_page))
				return false;
			if (!pml4_set_page(src_page->pml4, src_page->va, src_page->frame->kva, false))
				return false;
			if (!pml4_set_page(dst_page->pml4, dst_page->va, src_page->frame->kva, false))
				return false;

			// //  이미 claim된 페이지는 물리 메모리를 복사
			// if (!vm_alloc_page(type, src_page->va, src_page->writable))
			// {
			// 	return false;
			// }
			// if (!vm_claim_page(src_page->va))
			// {
			// 	return false;
			// }
			// struct page *dst_page = spt_find_page(dst, src_page->va);
			// memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
		}
	}
	return true;
}
/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->spt_hash, hash_destructor);
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

void hash_destructor(struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(page);
}