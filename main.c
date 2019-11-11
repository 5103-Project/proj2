/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include<time.h> 

#define FREE 0
#define TAKEN 1
#define THRESHOLD 6

int diskr_cnt;
int diskw_cnt;
int pagefault_cnt;
int npages;
int nframes;
const char* replaceMethod;
const char *program;
struct node;
struct flist;
struct flist* FL;
struct disk *disk;

struct node{
	int frame;
	struct node* next;
	struct node* pre;
	int page;
	int cnt; //used to implement clock algorithm in replacement policy
};

struct flist{
	struct node* head;
	struct node* tail;
	int* free;
	int nframes;
	int freeframes;
};

void flist_init(int nframes, struct flist* fl){
	fl->nframes = nframes;
	fl->freeframes = nframes;
	fl->head = NULL;
	fl->tail = NULL;
	fl->free = malloc(nframes * sizeof(int));
	//initialize free
	memset(fl->free, 0, nframes * sizeof(int));
}

int nextFree(){
	for(int i = 0; i < FL->nframes; i++){
		if(FL->free[i] == FREE){	
			return i;
		}
	}	
	return -1;
}

void setHold(int frame, int page){
	FL->free[frame] = TAKEN;
	struct node* new = malloc(sizeof(struct node));
	new->page = page;
	new->cnt = 0;
	new->frame = frame;
	if(FL->head == NULL){
		FL->head = new;
		FL->tail = new;
		new->pre = NULL;
		new->next = NULL;
	}else{
		new->pre = FL->tail;
		FL->tail->next = new;
		new->next = NULL;
		FL->tail = new;
	}
	if(FL->freeframes>0){
		FL->freeframes--;
	}
}

void clock_cnt(struct page_table *pt){
	struct node *tmp = FL->head;
	for(int i = 0 ; i < (FL->nframes - FL->freeframes); i++){
		if(pt->page_bits[tmp->page] & PROT_WRITE){
			tmp->cnt = 0;
		}
		else{
			tmp->cnt++;
		}
		tmp = tmp->next;
	}
}

struct node* rand_node(){
	int idx = rand() % FL->nframes;
	struct node *evict_node = FL->head;
	for(int i = 0; i < idx; i++){
		evict_node = evict_node->next;
	}
	return evict_node;
}

struct node* clock_node(){
	struct node* tmp = FL->head;
	struct node* evict_frame = FL->head;
	for(int i = 0; i < FL->nframes; i++){
		if(tmp->cnt >= THRESHOLD){
			evict_frame = tmp;
			return evict_frame;
		}
		tmp = tmp->next;
	}
	evict_frame = rand_node();
	return evict_frame;
}

struct node* fifo_node(){
	return FL->head;
}

void remove_node(struct node* evict_node){
	if(evict_node == FL->head){
		FL->head = evict_node->next;
		if(FL->head != NULL){
			FL->head->pre = NULL;
		}		
		FL->free[evict_node->frame] = FREE;
	}
	else if(evict_node == FL->tail){
		FL->tail = evict_node->pre;
		if(FL->tail != NULL){
			FL->tail->next = NULL;
		}		
		FL->free[evict_node->frame] = FREE;
	}
	else{
		struct node* tempPre = evict_node->pre;
		struct node* tempNext = evict_node->next;
		if(tempPre != NULL){
			tempPre->next = tempNext;
		}
		if(tempNext != NULL){
			tempNext->pre = tempPre;
		}
		FL->free[evict_node->frame] = FREE;
	}
}

void write_dirty(struct page_table *pt,struct node *evict_node, int *diskw_cnt){
	int frame = evict_node->frame;
	int page = evict_node->page;
	if(pt->page_bits[page] & PROT_WRITE){
			disk_write(disk,page,&(pt->physmem[frame*PAGE_SIZE]));
			(*diskw_cnt)++;
	}
}

void evict(struct page_table *pt, int *diskw_cnt){

	struct node* evict_node;
	if(!strcmp(replaceMethod, "fifo")){
		evict_node = fifo_node();	
	}
	else if(!strcmp(replaceMethod, "rand")){
		evict_node = rand_node();	
	}
	else if(!strcmp(replaceMethod, "clock")){
		evict_node = clock_node();
	}
	write_dirty(pt,evict_node,diskw_cnt);
	remove_node(evict_node);
	page_table_set_entry(pt, evict_node->page, 0, 0);
	free(evict_node);
}

void diskToMem(struct page_table *pt,int frame, int page){
	setHold(frame, page);
	page_table_set_entry(pt, page, frame, PROT_READ);
	disk_read(disk,page,&(pt->physmem[frame*PAGE_SIZE]));
	diskr_cnt++;
}

void page_fault_handler(struct page_table *pt, int page )
{
	//if the page bit is PROT_READ which means the page is already in physical memory, just add PROT_WRITE bit
	pagefault_cnt++;
	int frame;
	if(pt->page_bits[page] & PROT_READ){
		frame = pt->page_mapping[page];
		page_table_set_entry(pt, page, frame, PROT_READ | PROT_WRITE);	
	}
	//page is not in physical memory, have to read data from disk
	else{
		frame = nextFree();
		//there are still free frames, read from disk
		if(frame != -1){
			diskToMem(pt, frame, page);
		}
		//there is no free frames, evict one and write back to disk if it is dirty and then read from disk
		else{
			evict(pt,&diskw_cnt);
			frame = nextFree();
			diskToMem(pt, frame, page);
		}
	}
	if(!strcmp(replaceMethod, "clock")){
		clock_cnt(pt);
	}
}

int main( int argc, char *argv[] )
{	
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	diskr_cnt = 0;
	diskw_cnt = 0;
	pagefault_cnt = 0;

	npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	replaceMethod = argv[3];
	program = argv[4];

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}

	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	char *physmem = page_table_get_physmem(pt);

	FL = malloc(sizeof(struct flist));
	flist_init(nframes, FL);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_delete(pt);
	disk_close(disk);
	printf("program: %s  replaceAlgprithm: %s\n", program, replaceMethod);	
	printf("page fault: %d \n", pagefault_cnt);
	printf("disk read: %d \n",diskr_cnt);
	printf("disk write: %d \n", diskw_cnt);
	printf("------------------------------------------------------------\n");
	return 0;
}
