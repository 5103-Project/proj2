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

struct node;

struct flist;

struct node{
	int frame;
	struct node* next;
	struct node* pre;
	int page;
};

struct flist{
	struct node* head;
	struct node* tail;
	int* free;
	int nframes;
	
};


struct flist* FL;

static const char* replaceMethod;

void flist_init(int nframes, struct flist* fl){
	fl->nframes = nframes;
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
	//fifo
	struct node* new = malloc(sizeof(struct node));
	new->page = page;
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
	
	if(!strcmp(replaceMethod, "fifo")){
	}
}


void evict(struct page_table *pt){

	struct node* expired;
	if(!strcmp(replaceMethod, "fifo")){
		int frame = FL->head->frame;
		FL->free[frame] = FREE;
		//if(!strcmp(replaceMethod, "fifo")){
		expired = FL->head;
		FL->head = expired->next;
		FL->head->pre = NULL;
	}else if(!strcmp(replaceMethod, "rand")){
		int idx = rand() % FL->nframes;
		expired = FL->head;
		for(int i = 0; i < idx; i++){
			expired = expired->next;
		}

		if(expired == FL->head){
			FL->head = expired->next;
		}

		if(expired == FL->tail){
			FL->tail = expired->pre;
		}

		//remove expired from FL
		struct node* tempPre = expired->pre;
		struct node* tempPost = expired->next;
		if(tempPre != NULL){
			tempPre->next = tempPost;
		}
		if(tempPost != NULL){
			tempPost->pre = tempPre;
		}
		int frame = expired->frame;
		FL->free[frame] = FREE;
		
	}
	page_table_set_entry(pt, expired->page, 0, 0);
	free(expired);
}

int replace(struct page_table *pt, int page){
	evict(pt);
	int idx = nextFree();
	setHold(idx, page);
	return idx;
}

void page_fault_handler(struct page_table *pt, int page )
{
	//start my code here
	printf("page falue on page# %d\n", page);
	// write operation
	if(pt->page_bits[page] & PROT_READ){
		page_table_set_entry(pt, page, pt->page_mapping[page], PROT_READ | PROT_WRITE);	
	}
	else{
		int idx = nextFree();
		if(idx != -1){
			setHold(idx, page);	
			page_table_set_entry(pt, page, idx, PROT_READ);
		}else{
			int frame = replace(pt, page);
			page_table_set_entry(pt, page, frame, PROT_READ);
		}
	}
}

int main( int argc, char *argv[] )
{
	srand(time(0));
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	replaceMethod = argv[3];
	const char *program = argv[4];

	struct disk *disk = disk_open("myvirtualdisk",npages);
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

	return 0;
}
