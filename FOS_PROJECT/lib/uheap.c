#include <inc/lib.h>
#define BLOCK_MAX ((USER_HEAP_MAX - USER_HEAP_START) / PAGE_SIZE )
struct Block{
	struct Block *next, *prev;
	uint32 size ,start_address;

} Blocks[BLOCK_MAX + 5];
struct List{
	struct Block head , tail;
}free_block_list,allocated_block_list,not_used_list;
int memory_initialized = 0;
// malloc()
//	This function use FIRST FIT strategy to allocate space in heap
//  with the given size and return void pointer to the start of the allocated space

//	To do this, we need to switch to the kernel, allocate the required space
//	in Page File then switch back to the user again.
//
//	We can use sys_allocateMem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls allocateMem(struct Env* e, uint32 virtual_address, uint32 size) in
//		"memory_manager.c", then switch back to the user mode here
//	the allocateMem function is empty, make sure to implement it.

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
void initialize_memory()
{
	memory_initialized = 1;
	not_used_list.head.next = &Blocks[0];
	Blocks[0].prev = &not_used_list.head;
	not_used_list.tail.prev = &Blocks[BLOCK_MAX-2];
	Blocks[BLOCK_MAX-2].next = &not_used_list.tail;
	for ( int i = 1 ; i < BLOCK_MAX-1 ; i++)
	{
		Blocks[i].prev = &Blocks[i-1];
		Blocks[i-1].next = &Blocks[i];
	}
	free_block_list.head.next = free_block_list.tail.prev = &Blocks[BLOCK_MAX-1];
	Blocks[BLOCK_MAX-1].next = &free_block_list.tail;
	Blocks[BLOCK_MAX-1].prev = &free_block_list.head;
	Blocks[BLOCK_MAX-1].start_address = USER_HEAP_START;
	Blocks[BLOCK_MAX-1].size = BLOCK_MAX;
	allocated_block_list.head.next = &allocated_block_list.tail;
	allocated_block_list.tail.prev = &allocated_block_list.head;
}
void remove_block( struct Block *ptr)
{
	ptr->prev->next = ptr->next;
	ptr->next->prev = ptr->prev;
}
void move_block( struct Block *head , struct Block *ptr)
{
	ptr->next = head->next;
	head->next->prev = ptr;
	ptr->prev = head;
	head->next = ptr;
}
void create_block( uint32 address , uint32 size)
{
	struct Block *ptr = not_used_list.tail.prev;
	if(ptr == &not_used_list.head)
	{
		panic("create_block() can not find not used block !!");
	}
	ptr->start_address = address;
	ptr->size = size;
	remove_block(ptr);
	move_block(&allocated_block_list.head,ptr);
}
void* malloc(uint32 size)
{
	if(!memory_initialized)
		initialize_memory();
	uint32 need_size = ROUNDUP(size,PAGE_SIZE) / PAGE_SIZE;
	struct Block *cur_free = &free_block_list.head , *needed_block = NULL;
	int mn = 1e9;
	while(cur_free->next != &free_block_list.tail)
	{
		cur_free = cur_free->next;
		 if ( cur_free->size >= need_size)
		 {
			if(cur_free->size < mn)
				mn = cur_free->size, needed_block = cur_free;
		 }
	}
	if(needed_block == NULL)
		return NULL;
	uint32 va = needed_block->start_address;
	if(mn == need_size)
	{
		remove_block(needed_block);
		move_block(&not_used_list.head,needed_block);
	}
	else
	{
		needed_block->start_address += need_size * PAGE_SIZE;
		needed_block->size -= need_size;
		create_block(va , need_size);
	}
	sys_allocateMem(va,need_size);
	return (void *)va;
	//TODO: [PROJECT 2021 - [2] User Heap] malloc() [User Side]
	// Write your code here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");

	//This function should find the space of the required range
	//using the BEST FIT strategy
	//refer to the project presentation and documentation for details
}
int can_join( struct Block *ptr1 , struct Block *ptr2)
{
	return (ptr1->start_address + ptr1->size * PAGE_SIZE == ptr2->start_address);
}
void join( struct Block *ptr1 , struct Block *ptr2)
{
	ptr2->start_address -= ptr1->size * PAGE_SIZE;
	ptr2->size += ptr1->size;
}
void insert( struct Block *ptr1 , struct Block *ptr2 , struct Block *ptr3)
{
	ptr1->next = ptr2;
	ptr2->next = ptr3;
	ptr3->prev = ptr2;
	ptr2->prev = ptr1;
}
// free():
//	This function frees the allocation of the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from page file and main memory then switch back to the user again.
//
//	We can use sys_freeMem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls freeMem(struct Env* e, uint32 virtual_address, uint32 size) in
//		"memory_manager.c", then switch back to the user mode here
//	the freeMem function is empty, make sure to implement it.

void free(void* virtual_address)
{
	uint32 va = ROUNDDOWN((uint32)virtual_address,PAGE_SIZE);
	struct Block *cur_allocated = &allocated_block_list.head , *needed_block = NULL;
	while(cur_allocated->next != &allocated_block_list.tail)
	{
		cur_allocated = cur_allocated->next;
		if(cur_allocated->start_address == va)
		{
			needed_block = cur_allocated;
			break;
		}
	}
	if(needed_block == NULL)
		return;
	uint32 free_size = needed_block->size;
	remove_block(needed_block);
	struct Block *cur_free = &free_block_list.head , *needed2_block = NULL;
	while(cur_free->next != &free_block_list.tail)
	{
		cur_free = cur_free->next;
		if((uint32)cur_free->start_address > va)
		{
			needed2_block = cur_free;
			break;
		}
	}
	if(needed2_block == NULL)
	{
		insert(free_block_list.tail.prev,needed_block,&free_block_list.tail);
	}
	else
	{
		if(can_join(needed_block,needed2_block))
		{
			join(needed_block,needed2_block);
			move_block(&not_used_list.head,needed_block);
			needed_block = needed2_block;
		}
		else
		{
			insert(needed2_block->prev,needed_block,needed2_block);
		}
		if(needed_block->prev != &free_block_list.head && can_join(needed_block->prev,needed_block))
		{
			struct Block *temp = needed_block->prev;
			join(temp,needed_block);
			remove_block(temp);
			move_block(&not_used_list.head,temp);
		}
	}
	sys_freeMem(va, free_size);
	//TODO: [PROJECT 2021 - [2] User Heap] free() [User Side]
	// Write your code here, remove the panic and write your code
    //panic("free() is not implemented yet...!!");

	//you should get the size of the given allocation using its address

	//refer to the project presentation and documentation for details
}

//==================================================================================//
//================================ OTHER FUNCTIONS =================================//
//==================================================================================//

void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	panic("this function is not required...!!");
	return 0;
}

void* sget(int32 ownerEnvID, char *sharedVarName)
{
	panic("this function is not required...!!");
	return 0;
}

void sfree(void* virtual_address)
{
	panic("this function is not required...!!");
}

void *realloc(void *virtual_address, uint32 new_size)
{
	panic("this function is not required...!!");
	return 0;
}

void expand(uint32 newSize)
{
	panic("this function is not required...!!");
}
void shrink(uint32 newSize)
{
	panic("this function is not required...!!");
}

void freeHeap(void* virtual_address)
{
	panic("this function is not required...!!");
}
