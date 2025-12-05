/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"
#include "include/vm/uninit.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/mmu.h" // pml4 관련 함수가 필요할 경우

static bool is_stack_growth_condition(struct intr_frame *f, void *page_addr);
static uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED);
static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

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

	/* TODO: Create the page, fetch the initialier according to the VM type,
	 * TODO: and then create "uninit" page struct by calling uninit_new. You
	 * TODO: should modify the field after calling the uninit_new. */
	/* TODO: Insert the page into the spt. */

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		struct page *p = (struct page *)malloc(sizeof(struct page));
		if (p == NULL)
		{
			return false;
		}
		bool (*initializer)(struct page *, enum vm_type, void *);
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			initializer = anon_initializer;
			break;
		case VM_FILE:
			initializer = file_backed_initializer;
			break;
		default:
			goto err;
		}
		uninit_new(p, upage, init, type, aux, initializer);
		p->writable = writable;
		if (!spt_insert_page(spt, p))
		{
			goto err;
		}
		return true;
	err:
		free(p);
		return false;
	}
	return false;
}

/* vm/vm.c */

struct page *spt_find_page(struct supplemental_page_table *spt, void *va)
{
	struct page page; // 중요: 포인터(*page)가 아니라 구조체 변수(page)로 선언해야 함

	/* 검색할 가상 주소 설정 (반드시 페이지 단위로 정렬) */
	page.va = pg_round_down(va);

	/* 해시 테이블에서 검색 */
	/* &page.hash_elem을 통해 스택에 있는 유효한 주소를 전달해야 함 */
	struct hash_elem *e = hash_find(&spt->hash, &page.hash_elem);

	/* 결과 반환 */
	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	return hash_insert(&spt->hash, &page->hash_elem) == NULL;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
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
	void *kva = palloc_get_page(PAL_USER);
	if (kva == NULL)
	{
		/*
		구현 필요
		*/
		printf("페이지 교체 구현");
		palloc_free_page(kva);
	}
	frame = (struct frame *)malloc(sizeof(struct frame));
	if (frame == NULL)
	{
		palloc_free_page(kva); // 구조체 할당 실패 시 물리 페이지도 반납
		return NULL;
	}
	frame->kva = kva;
	frame->page = NULL;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *addr UNUSED)
{
	void *stack_page = pg_round_down(addr);
	if (vm_alloc_page(VM_ANON, stack_page, true))
	{
		vm_claim_page(stack_page);
	}
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
	struct page *page = NULL;

	/* 1. 주소 유효성 검사 (커널 주소이거나 NULL이면 차단) */
	if (is_kernel_vaddr(addr) || addr == NULL)
	{
		return false;
	}

	/* 2. SPT에서 페이지 검색 및 지연 로딩 (Lazy Loading) */
	void *page_addr = pg_round_down(addr);
	page = spt_find_page(spt, page_addr);

	if (page != NULL)
	{
		/* 읽기 전용 페이지에 쓰기 시도를 하면 차단 */
		if (write && !page->writable)
		{
			return false;
		}
		return vm_do_claim_page(page);
	}

	/* 3. 스택 확장 (Stack Growth) */
	/* 주의: is_stack_growth_condition 내부에서 user/kernel 모드에 따른 RSP 처리가 되어 있어야 함 */
	if (is_stack_growth_condition(f, addr))
	{
		/* 스택 확장을 요청 */
		vm_stack_growth(page_addr);

		/* [중요 수정] 확장이 실제로 성공했는지 확인해야 함 */
		/* vm_stack_growth는 void형이므로, spt_find_page로 재검색하여 확인 */
		page = spt_find_page(spt, page_addr);
		if (page == NULL)
		{
			return false; // 할당 실패 시 false 반환하여 커널 패닉 유도
		}

		/* 페이지가 생성되었다면 물리 프레임 매핑 시도 */
		return vm_do_claim_page(page);
	}

	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
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
	/* 1. 프레임 할당 */
	struct frame *frame = vm_get_frame();
	if (frame == NULL)
	{
		return false;
	}

	/* 2. 구조체 연결 (Link the page and frame) */
	/* TODO: page와 frame을 서로 연결하십시오. */
	frame->page = page;
	page->frame = frame;
	/* 3. 하드웨어 페이지 테이블 매핑 */
	struct thread *t = thread_current();
	/* TODO: pml4_set_page를 호출하여 매핑하십시오. */
	/* 만약 매핑 실패 시(return false), 프레임을 해제하고 false를 반환해야 합니다. */
	if (!pml4_set_page(t->pml4, page->va, frame->kva, page->writable))
	{
		/* 실패 시 프레임 반환 로직 필요 (예: vm_dealloc_frame(frame) 등)
		   현재 단계에서는 단순히 return false 처리 */
		return false;
	}

	/* 4. 데이터 로드 (Swap In) */
	/* 이 함수가 true를 반환해야 최종 성공입니다. */
	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(&spt->hash, page_hash, page_less, NULL);
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
/* hash_hash_func 규격에 맞게 수정 */
static uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED)
{
	const struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

/* page_less는 기존 로직이 맞으나, 일관성을 위해 재확인 */
bool page_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);
	return a->va < b->va;
}
static bool is_stack_growth_condition(struct intr_frame *f, void *fault_addr)
{
	uint64_t rsp;
	if (is_user_vaddr(f->rsp))
	{
		rsp = f->rsp;
	}
	else
	{
		rsp = thread_current()->tf.rsp;
	}
	if (fault_addr < USER_STACK - (1 << 20) || (uint64_t)fault_addr >= USER_STACK)
	{

		return false;
	}
	else if (fault_addr >= rsp - 8)
	{
		return true;
	}
	return false;
}
