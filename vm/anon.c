/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static struct bitmap *swap_table;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void)
{
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	if (swap_disk == NULL)
	{
		return;
	}
	// swap disk의 크기를 페이지 단위로 계산
	// 1 sector = 512 bytes, 1 page = 4096 bytes = 8 sectors
	size_t swap_size = disk_size(swap_disk) / 8;
	swap_table = bitmap_create(swap_size);
	if (swap_table == NULL)
	{
		PANIC("swap_table creation failed");
	}
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index = -1;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in(struct page *page, void *kva)
{
	struct anon_page *anon_page = &page->anon;

	if (anon_page->swap_index == -1)
	{
		// New page that hasn't been swapped out yet - just zero it
		memset(kva, 0, PGSIZE);
		return true;
	}

	// swap disk에서 페이지 읽기 (1 page = 8 sectors)
	for (int i = 0; i < 8; i++)
	{
		disk_read(swap_disk, anon_page->swap_index * 8 + i, kva + i * DISK_SECTOR_SIZE);
	}

	// swap table에서 해당 슬롯 해제
	bitmap_set(swap_table, anon_page->swap_index, false);
	anon_page->swap_index = -1;

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out(struct page *page)
{
	struct anon_page *anon_page = &page->anon;

	// swap table에서 빈 슬롯 찾기
	size_t swap_index = bitmap_scan(swap_table, 0, 1, false);
	if (swap_index == BITMAP_ERROR)
	{
		return false; // swap disk가 가득 참
	}

	// swap disk에 페이지 쓰기 (1 page = 8 sectors)
	for (int i = 0; i < 8; i++)
	{
		disk_write(swap_disk, swap_index * 8 + i, page->frame->kva + i * DISK_SECTOR_SIZE);
	}

	// swap table에 표시하고 인덱스 저장
	bitmap_set(swap_table, swap_index, true);
	anon_page->swap_index = swap_index;

	// 페이지 테이블에서 매핑 제거
	pml4_clear_page(page->pml4, page->va);

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy(struct page *page)
{
	struct anon_page *anon_page = &page->anon;

	// 페이지가 swap disk에 있으면 해당 슬롯 해제
	if (anon_page->swap_index != -1)
	{
		bitmap_set(swap_table, anon_page->swap_index, false);
		anon_page->swap_index = -1;
	}
	if (page->frame == NULL)
		return;
	page->frame->ref_count--;
	// 페이지가 메모리에 있으면 프레임 해제
	if (page->frame->ref_count < 1)
	{
		pml4_clear_page(page->pml4, page->va);
		list_remove(&page->frame->frame_elem);
		palloc_free_page(page->frame->kva);
		free(page->frame);
		page->frame = NULL;
	}
}
