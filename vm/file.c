/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"
#include "userprog/syscall.h"
static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page = &page->file;

	// 파일에서 페이지 데이터 읽기
	lock_acquire(&filesys_lock);
	file_seek(file_page->file, file_page->offset);
	int bytes_read = file_read(file_page->file, kva, file_page->page_read_bytes);
	lock_release(&filesys_lock);

	// 파일이 비어있거나 짧은 경우, 읽은 만큼만 사용하고 나머지는 0으로 채움
	if (bytes_read < (int)file_page->page_read_bytes)
	{
		memset(kva + bytes_read, 0, PGSIZE - bytes_read);
	}
	else
	{
		// 나머지 부분은 0으로 채움
		memset(kva + file_page->page_read_bytes, 0, PGSIZE - file_page->page_read_bytes);
	}
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page = &page->file;

	// Dirty bit 확인
	if (pml4_is_dirty(page->pml4, page->va))
	{
		// 파일에 write-back
		lock_acquire(&filesys_lock);
		file_write_at(file_page->file, page->frame->kva, file_page->page_read_bytes, file_page->offset);
		pml4_set_dirty(page->pml4, page->va, 0);
		lock_release(&filesys_lock);
	}

	// 페이지 테이블 엔트리 제거
	pml4_clear_page(page->pml4, page->va);
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page = &page->file;

	// 페이지가 메모리에 로드되어 있으면 write-back
	if (page->frame != NULL && page->writable)
	{
		// Dirty bit 확인 - 수정된 경우에만 write-back
		if (pml4_is_dirty(page->pml4, page->va))
		{
			lock_acquire(&filesys_lock);
			file_write_at(file_page->file, page->frame->kva, file_page->page_read_bytes, file_page->offset);
			lock_release(&filesys_lock);
		}
	}

	// 파일 핸들 닫기 (메모리에 있든 없든 항상 닫아야 함)
	if (file_page->file != NULL)
	{
		lock_acquire(&filesys_lock);
		file_close(file_page->file);
		lock_release(&filesys_lock);
		file_page->file = NULL;
	}
}

/* Do the mmap */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
}

/* Do the munmap */
void do_munmap(void *addr)
{
}
