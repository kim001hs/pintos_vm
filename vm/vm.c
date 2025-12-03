/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/lib/kernel/hash.h"

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
		
		/* TODO: Insert the page into the spt. */
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va) //페이지폴트 발생 시 이거로 spt 뒤짐
{	/* 기능: va -> [ SPT 뒤져 page 찾기 ] -> page */
	/* TODO: Fill this function. */

	// 0: va를 페이지단위로 정렬 ((선택))
	va = pg_round_down(va);

	// 1: va 값만 넣은 임시 빈 페이지 만들기
	struct page p;
	p.va = va;
	
	// 2: 만든 빈 페이지의 hash_elem을 보내 실제 page구조체 찾기
	struct hash_elem *e = hash_find(&spt->spt_map, &p.h_elem);
	if (e == NULL) {
		return NULL;
	}
	struct page *page = hash_entry(e, struct page, h_elem);
	
	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt,
					 struct page *page)
{ // spt에 페이지 등록 (단, 해당 페이지의 va가 이미 존재하는지 확인)
	int succ = false;
	/* TODO: Fill this function. */
	// 페이지 등록 ========
	if (!hash_insert(&spt->spt_map, &page->h_elem)){ //동일요소 테이블에 없는 경우
		succ = true;
	}
	return succ;
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

/* palloc() 하고 프레임을 가져와라. 
사용 가능한 페이지가 없다면, 페이지를 축출(evict)하고 그것을 반환하라. 
이 함수는 항상 유효한 주소를 반환한다. 
즉, 유저 풀 메모리가 가득 찼을 경우, 
이 함수는 이용 가능한 메모리 공간을 얻기 위해 프레임을 축출한다*/
static struct frame *
vm_get_frame(void)
{ // 기능: frame 할당받기(페이지,sva 포함) -> 초기화하여 프레임 반환
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	// 1: palloc으로 페이지 할당받기
	struct page *p = palloc_get_page(PAL_USER);
	if (p == NULL){
		PANIC("[vm_get_frame] todo"); //추후 swap-out 구현 필요
	}

	// 2: 프레임도 할당받고 멤버들 초기화
	frame = malloc(sizeof(struct frame));
	frame->page = p;
	//frame->kva = ??;

	// 3: 프레임 반환
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
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	// 1 - SPT에서 오류 발생한 페이지 찾음 > 메모리 참조 유효한지 확인 > 데이터 찾기 (파일 시스템/스왑 슬롯에서)
	
	// 2 - 페이지를 저장할 프레임 확보

	// 3 - 데이터 프레임 가져오기(파일 / 스왑슬롯에서)

	// 4 - 오류가 발생한 가상주소의 PT 항목을 물리적 페이지로 지정

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
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{	//기능: 페이지->프레임 할당받아(=vm_get_frame()) mmap에 등록(=pte추가) => 성공여부 반환
	// 1: 프레임 할당받기(페이지 포함) ========
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */

	// 2: PTE 등록 (va-pa) =mmap)  ========
	pml4_set_page(&thread_current()->pml4, page, frame->page);

	return swap_in(page, frame->kva);
}
/* ======== 해시 헬퍼 함수 ======== */
// page_hash() : VA값 -> 해시치 생성 (해시의 키를 va로 설정) >> hash_init에서 호출됨
unsigned page_hash(const struct hash_elem *e, void *aux UNUSED){
	const struct page *p = hash_entry(e, struct page, h_elem);
	return hash_bytes(&p->va, sizeof(p->va));
} //-> 식별기준: page->va만으로 가능
// 두 페이지 중 어떤 VA가 더 작은지 리턴 (정렬용)
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	const struct page *pa = hash_entry(a, struct page, h_elem);
	const struct page *pb = hash_entry(b, struct page, h_elem);

	return pa->va < pb->va;
}
//  ( hash_apply()에 보내 각 해시테이블 요소들에 적용할 예정 )
void get_each_hash(struct hash_elem *e, void *aux){
	
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	// 호출되는 곳: 새 프로세스 시작될 때(initd()), 프로세스 포크될 때(__do_fork())
	if (!hash_init(&spt->spt_map, page_hash, page_less, NULL)){
		printf("\n[vm.c] spt_init failed!\n\n");
	}
}

/* 해시테이블 복사 위한 헬퍼 함수 */
//typedef void hash_action_func(struct hash_elem *e, void *aux);


/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{ //목적: fork할 때 부모 SPT를 자식 SPT에 복사하는 것 >> 완전 초기화부터 다 진행하기
	// new SPT 초기화
	if (!hash_init(&dst->spt_map, page_hash, page_less, NULL)){
		return false;
	}
	// 각 해시 iter 돌면서 복사 > list는 어케 복사해 ? 그 안에서 또?
	
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
