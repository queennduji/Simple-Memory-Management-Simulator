#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
Queen Nduji
CIS 545
10-30-17
*/

char* memory;
char* pagetable;
int pages;
int remainingpages;
int numprocesses = 0;
int failedallocations = 0;
int allocated = 0;
char mode;
int total_size;

typedef struct process { //This is process node for SEGMENTATION.
	struct process *next;
	int bytes;
	int pid;
	int text_size;
	int text_allocated;
	int data_size;
	int data_allocated;
	int heap_size;
	int heap_allocated;
	int text_location;
	int data_location;
	int heap_location;
	int sumallocated;
	
} process_t;

process_t *Phead = NULL;
process_t **Phelper;

typedef struct node { //This is process node for PAGING.
	struct node* next;
	int pid;
	int bytes;
	char* virtualptable;
	
} node_t;

node_t * head = NULL;

//Hole stuff starts here.-------------------------------------------------------------

typedef struct hole {
	struct hole *next; //USED FOR LINKED LIST
	struct hole *high; //USED FOR TREE
	struct hole *low;  //USED FOR TREE
	int holestart;
	int holesize;
} hole_t;

hole_t **helper;
int numholes;

hole_t* small(hole_t* root) { 
/*
	Find the smallest node of the high subtree of the given node.
*/
	hole_t *temp = root->high;
	helper = &root->high;
	while(temp->low != NULL) {
		helper = &temp->low;
		temp = root->low;
	}
	return temp;
}

hole_t* big(hole_t *root) {
/*
	Find the largest node of the low subtree of the given node.
*/
	hole_t *temp = root->low;
	helper = &root->low;
	while(temp->high != NULL) {
		helper = &temp->high;
		temp = temp->high;
	}
	return temp;
}

hole_t *Hhead = NULL;
hole_t *listHead = NULL;

int addlist(hole_t *newHole) {
/*
	Add the given hole to the linked list.
*/
	hole_t *temp = listHead;
	numholes++;
	if(listHead == NULL) {
		listHead = newHole;
		return 1;
	}
	if(newHole->holestart < listHead->holestart) {
		newHole->next = listHead;
		listHead = newHole;
		return 1;
	}
	while(temp->next != NULL && temp->next->holestart < newHole->holestart) {
		temp = temp->next;
	}	
	newHole->next = temp->next;
	temp->next = newHole;
	return 1;
}

hole_t* removelist(int targetStart) {
/*
	Remove a hole from the linked list with the given starting value.
*/
	hole_t *temp = listHead;
	helper = &listHead;
	while(temp->holestart != targetStart) {
		helper = &temp->next;
		temp = temp->next;
	}
	*helper = temp->next;
	numholes--;
	return temp;
}

hole_t* makehole(int size, int start) {

/*
	Make a hole and add it to the binary search tree.
	Call the function to add the hole to the linked list.
*/
	hole_t *temp = Hhead;
	if(Hhead == NULL) {
		Hhead = malloc(sizeof(hole_t));
		Hhead->holestart = start;
		Hhead->holesize = size;
		addlist(Hhead);
		return Hhead;
	}
	while(1) {
		if(size >= temp->holesize) {
			if(temp->high == NULL) {
				temp->high = malloc(sizeof(hole_t));
				temp = temp->high;
				temp->holesize = size;
				temp->holestart = start;
				addlist(temp);
				return temp;
			}
			temp = temp->high;
		}
		if(size < temp->holesize) {
			if(temp->low == NULL) {
				temp->low = malloc(sizeof(hole_t));
				temp = temp->low;
				temp->holesize = size;
				temp->holestart = start;
				addlist(temp);
				return temp;
			}
			temp = temp->low;
		}
	}
}

void printTree(hole_t *temp) {

/*
	Print the binary search tree of holes.
	The holes will be printed in order of largest hole to smallest hole.
*/
	if(temp == NULL)
		return;
	printTree(temp->high);
	printf("%d %d\n", temp->holesize, temp->holestart);
	printTree(temp->low);
}

hole_t* removeNode(int size, int start) {

/*
	Remove a hole from the tree with the given size/start.
	No error handling is needed because it will only be called
	from functions that know the exact values for the target node.
	At the end, call the function to remove the hole from the linked list.
*/
	hole_t *temp = Hhead; hole_t *temp2 = Hhead;
	hole_t **pptr = &Hhead;
	char high = 0; //This represents whether something is the parent's high or low child.
	while(1) {
		if(size > temp->holesize) { 
			pptr = &temp->high;
			high = 1;
			temp = temp->high;
			
			continue;
		}
		if(size < temp->holesize) {
			pptr = &temp->low; 
			high = 0;
			temp = temp->low;
			continue;
		}
		if (size == temp->holesize && temp->holestart == start) {
			break;
		}

		if(size == temp->holesize && temp->holestart != start) {
			pptr = &temp->high;
			temp = temp->high;
		}
	}
	//TEMP IS THE TARGET NODE. pptr points towards the pointer of the parent towards the target node.
	if(temp->high == NULL && temp->low == NULL) {
		*pptr = NULL;
	}
	else if(temp->high == NULL) {
		*pptr = temp->low;
	}
	else if(temp->low == NULL) {
		*pptr = temp->high;
	}
	else if(high == 1) {
		*pptr = small(*pptr); //helper is the reference to the node replacing TEMP.
		*helper = NULL;
		(*pptr)->low = temp->low;
		(*pptr)->high = temp->high;
	}
	else if(high == 0) {
		*pptr = big(*pptr); //helper is the reference to the node replacing TEMP.
		*helper = NULL;
		(*pptr)->high = temp->high;
		(*pptr)->low = temp->low;
	}
	return removelist(temp->holestart);
}

void printList(hole_t *temp) {
/*
	Print the list of holes. The list is sorted by starting location in memory.
*/
	int i = 1;
	while(temp != NULL) {
		printf("hole %d: start location = %d, size = %d\n", i, temp->holestart, temp->holesize);
		temp = temp->next;
		i++;
	}
}


hole_t *findhole(int size) {
/*
	Best Fit implementation, search the binary tree for
	the smallest node which will fit the incoming size.
	When a possible hole is found, remember it.
	Return the last hole remembered.
*/
	hole_t *temp = Hhead, *lasthole = NULL; helper = &Hhead;
	while(temp != NULL) {
		if(temp->holesize > size) { 
			lasthole = temp;
			helper = &temp->low;
			temp = temp->low;
			continue;
		}
		if(temp->holesize < size) {
			helper = &temp->high;
			temp = temp->high;
			continue;
		}
		if(temp->holesize == size) {
			lasthole = temp;
			break;
		}
	}
	return lasthole;
}

int allocatespace(int size, int *allocated2) {
/*
	Find a hole and remove the space required for the process segment.
	Set the pointer to allocated equal to the amount allocated.
	Return -1 if there is no hole with a large enough size available.
*/
	hole_t *temp = findhole(size); //findhole(int) is an implementation of Best Fit
	if(temp == NULL) {
		return -1;
	}
	if(temp->holesize - size <= 16) {
		removeNode(temp->holesize, temp->holestart);
		*allocated2 = temp->holesize;
		return temp->holestart;
	}
	removeNode(temp->holesize, temp->holestart);
	temp = makehole(temp->holesize - size, temp->holestart);
	*allocated2 = size;
	return temp->holestart + temp->holesize;
}

int conjoin () {
/*
	Free block coalescing.
	The linked list is sorted by where a hole starts.
	If holestart+holesize = the next holestart
	conjoin the two holes, do this for the entire list.
*/
	int a, b;
	hole_t *temp = listHead, *temp2;
	while(temp->next != NULL) {
		if(temp->holestart + temp->holesize == temp->next->holestart) {
			a = temp->holesize + temp->next->holesize;
			b = temp->holestart;
			removeNode(temp->next->holesize, temp->next->holestart);//Program bricks on this line.
			removeNode(temp->holesize, temp->holestart);
			temp = makehole(a, b);
		} else {
		temp = temp->next;
		}
	}
}

int allocateSegment(int bytes, int pid, int text_size, int data_size, int heap_size) {
/*
	Allocate the text, data, heap for an incoming process.
	Add process to process list.
	If there is a segmentation fault, increment failedallocations
	and undo previous allocations/remove the failed process from
	the process list.
*/
	int flag = 0;
	process_t *temp = Phead;
	Phelper = &Phead;
	if(Phead == NULL) {
		Phead = malloc(sizeof(process_t));
		Phead->pid = pid;
		Phead->text_size = text_size;
		Phead->data_size = data_size;
		Phead->heap_size = heap_size;
		Phead->bytes = bytes;
		Phead->text_location = allocatespace(text_size, &Phead->text_allocated);
		Phead->data_location = allocatespace(data_size, &Phead->data_allocated);
		Phead->heap_location = allocatespace(heap_size, &Phead->heap_allocated);
		Phead->sumallocated = Phead->heap_allocated + Phead->data_allocated + Phead->text_allocated;
		if(Phead->text_location == -1 || Phead->data_location == -1 || Phead->heap_location == -1) {
			if(Phead->heap_location != -1) {
				makehole(Phead->heap_allocated, Phead->heap_location);
			}
			if(Phead->data_location != -1) {
				makehole(Phead->data_allocated, Phead->data_location);
			}
			if(Phead->text_location != -1) {
				makehole(Phead->text_allocated, Phead->text_location);
			}
			printf("SEGMENTATION FAULT WITH PROCESS %d, NO HOLE LARGE ENOUGH\n", pid);
			failedallocations++;
			Phead = NULL;
			conjoin();
			return -1;
		}
		allocated = allocated + Phead->sumallocated;
		numprocesses++;
		return 1;
	}
	while(temp->next != NULL) {
		temp = temp->next;
		Phelper = &temp->next;
	} //TEMP IS THE END OF PROCESS LIST
	temp->next = malloc(sizeof(process_t));
	temp = temp->next;
	temp->pid = pid;
	temp->text_size = text_size;
	temp->data_size = data_size;
	temp->heap_size = heap_size;
	temp->bytes = bytes;
	temp->text_location = allocatespace(text_size, &temp->text_allocated);
	temp->data_location = allocatespace(data_size, &temp->data_allocated);
	temp->heap_location = allocatespace(heap_size, &temp->heap_allocated);
	temp->sumallocated = temp->heap_allocated + temp->data_allocated + temp->text_allocated;
	if(temp->text_location == -1 || temp->data_location == -1 || temp->heap_location == -1) {
			if(temp->heap_location != -1) {
				makehole(temp->heap_allocated, temp->heap_location);
			}
			if(temp->data_location != -1) {
				makehole(temp->data_allocated, temp->data_location);
			}
			if(temp->text_location != -1) {
				makehole(temp->text_allocated, temp->text_location);
			}
			printf("SEGMENTATION FAULT WITH PROCESS PID: %d, NO HOLE LARGE ENOUGH\n", pid);
			failedallocations++;
			conjoin();
			*Phelper = NULL;
			return -1;
		}
	allocated = allocated + temp->sumallocated;
	numprocesses++;
	return 1;
}

int deallocateSegment(int pid) {
/*
	Deallocates a process using segmentation.
*/
	process_t *temp = Phead;
	Phelper = &Phead;
	while(temp != NULL && temp->pid != pid) {
		Phelper = &temp->next;
		temp = temp->next;
	}
	if(temp == NULL) {
		printf("There is no process with PID: %d to be deallocated.\n", pid);
		return -1;
	}
	//TEMP IS NOW THE TARGET PROCESS
	*Phelper = temp->next;
	makehole(temp->text_allocated, temp->text_location);
	makehole(temp->data_allocated, temp->data_location);
	makehole(temp->heap_allocated, temp->heap_location);
	conjoin();
	allocated = allocated - temp->sumallocated;
	numprocesses--;
	return 1;	
}

//Hole stuff ends here.------------------------------------------------------------------------------------

void printProcess() {
/*
	Prints the list of processes and their related data.
*/
	process_t *temp = Phead;
	printf("Process list:\n");
	while(temp != NULL) {
		printf("process id=%d, size=%d allocation=%d\n", temp->pid, temp->bytes, temp->sumallocated);
		printf("	text start=%d, size=%d\n", temp->text_location, temp->text_allocated);
		printf("	data start=%d, size=%d\n", temp->data_location, temp->data_allocated);
		printf("	heap start=%d, size=%d\n", temp->heap_location, temp->heap_allocated);
		temp = temp->next;
	}
}


void MemoryManager (int bytes, int policy) {
/*
	Initialize data with the correct number of bytes.
	Set policy for Segmentation or Paging.
*/

	if (policy == 1) {
		pages = bytes/32; //How many pages are being initialized.
		remainingpages = pages;
		pagetable = (char*) malloc(pages);
		memset(pagetable, 0x00, pages);
		return;
	}
	total_size = bytes;
	mode = policy;
	makehole(bytes, 0);
}



int allocateHelper(char *a){
/*
	Extract integer values from the input string, and feed those integers to the correct allocate call.
*/
	int i = 0, allocations[5];
	char b[25];
	b[0] = 0;
	for(i = 0; i < 5; i++) {
		a = &a[strlen(b)];
		sscanf(a, "%s", b);
		allocations[i] = atoi(b);
		strcat(b, " ");
	}
	if(mode == 0) {
		return allocateSegment(allocations[0],allocations[1],allocations[2],allocations[3],allocations[4]);
	}
	return allocate(allocations[0],allocations[1],allocations[2],allocations[3],allocations[4]);
}
int allocate(int bytes, int pid, int text_size, int data_size, int heap_size) {
/*
	Allocates pages to an incoming process with n bytes.
	Adds incoming process to process list.
*/
	if(bytes == 0) {
		printf("Cannot allocate processes with 0 bytes.\n");
		return -1;
	}

	int i, j, reqpages = (bytes+31)/32;

	if(reqpages>remainingpages) {
		printf("Not enough remaining pages to allocate for process %d.\n", pid);
		failedallocations++;
		return -1;
	}

	if (head == NULL) {
		head = malloc(sizeof(node_t));
		head->pid = pid;
		head->bytes = bytes;
		head->virtualptable = malloc((bytes + 31)/32);
		for(i = 0; i < (bytes + 31)/32; i++) {//i is virtual page number.
			for(j = 0; j < pages; j++) { //j is physical page number.
				if(pagetable[j] == 0) {
					head->virtualptable[i] = j;
					pagetable[j] = 1; //A value of 1 in the pagetable means that the page is occupied, 0 is unoccupied.
					break;
				}
			}
		}
	
	}
	else {

		node_t* current = head;
		while(current->next != NULL) {
				if(current->pid == pid) {
					printf("There is already a process with pid: %d\n", pid);
					failedallocations++;
					return -1;
				}
				current = current->next;
			}
			if(current->pid == pid) {
				printf("There is already a process with pid: %d\n", pid);
				failedallocations++;
				return -1;
			}
		current->next = malloc(sizeof(node_t));
		current = current->next;
		current->pid = pid;
		current->bytes = bytes;
		current->virtualptable = malloc((bytes + 31)/32);
		for(i = 0; i < (bytes + 31)/32; i++) {
			for(j = 0; j < pages; j++) {
				if(pagetable[j] == 0) {
					current->virtualptable[i] = j;
					pagetable[j] = 1;
					break;
				}
			}
		}
	}
		remainingpages = remainingpages - reqpages;
		numprocesses++;
}

int deallocate(int pid) {
/*
	Remove process from process list.
	Set pages associated with the removed process to available.
	Call segment version if mode == 0.
*/
	if(mode == 0) {
		return deallocateSegment(pid);
	}
	node_t* current = head, *temp = head;
	int i;

	while(current != NULL) { 
		if(current->pid == pid) {
			remainingpages = remainingpages + ((current->bytes+31)/32);
			for(i = 0; i < ((current->bytes+31)/32); i++) {
				pagetable[current->virtualptable[i]] = 0; //Opens pages in pagetable
			}
			break;
			
		}

		current = current->next;

	} //END OF LOOP
	if(current == NULL) {
		printf("Process with PID: %d doesn't exist.\n", pid);
		return -1;
	}
	if(numprocesses > 2 && head->pid != pid) {
		while(temp->next->pid != pid) {
			temp = temp->next;
		}
	}
	if(head->pid == pid) { 
		head = head->next; //Removes the head node.
	}
	else {
		temp->next = current->next; //Remove the target node.
	}
	numprocesses--;
	return 1;
}

void printMemoryStateSegment() {
/*
	Print the memory state using the segment version.
*/
	printf("Memory size = %d bytes, allocated = %d bytes, free = %d\n", total_size, allocated, total_size - allocated);
	printf("There are currently %d holes, and %d active processes\n", numholes, numprocesses);
	printf("Hole list:\n");
	printList(Hhead);
	printProcess();
	printf("Failed allocations (No memory) = %d\n", failedallocations);
}

void printMemoryState() {
/*
	Print the memory state, call segment version if mode is 0.
*/
	if(mode == 0) {
		return printMemoryStateSegment();
	}
	int i;
	node_t* current = head;
	printf("Memory size = %d bytes, total pages %d\n", pages*32, pages);
	printf("allocated pages = %d, free pages = %d\n", pages - remainingpages, remainingpages);
	printf("There are currently %d active process\n", numprocesses);
	printf("Free Page list:\n   ");
	for(i = 0; i < pages; i++) {
		if(pagetable[i] == 0)
			printf("%d, ", i);
	}
	printf("\n");
	printf("Process list:\n");
	while(current != NULL){
		printf(" Process id=%d, size=%d, number of pages=%d\n", current->pid, current->bytes, (current->bytes+31)/32);

		for(i = 0; i < (current->bytes+31)/32 - 1; i++) {
			printf("  Virt Page %d -> Phys Page %d used: %d bytes\n", i,current->virtualptable[i], 32);
		}
		if(current->bytes % 32 == 0){
			printf("  Virt Page %d -> Phys Page %d used: %d bytes\n", i,current->virtualptable[i], 32);
		}
		else {
			printf("  Virt Page %d -> Phys Page %d used: %d bytes\n", i,current->virtualptable[i], current->bytes % 32);
		}
		current = current->next;
	}
	printf("Failed allocations (No memory) = %d\n", failedallocations);
}

int interpreter(char *a) {
/*
	This function interperets a string and forwards it to the correct function.
*/
char b[25];
if(a[0] == 'A') {
	allocateHelper(a+2);
	return 1;
	}
if(a[0] == 'D') {
	deallocate(atoi(a+2));
	return 1;
	}
if(a[0] == 'P') {
	printMemoryState();
	return 1;
	}
sscanf(a, "%s", b);
mode = (a + strlen(b)+1)[0];
MemoryManager(atoi(b), mode - 48);
}

int main(int argc, char *argv[]) {
	char commands[25];
	int i;	
	FILE *input = fopen("test", "r");
	while(fgets(commands, 25, input) != NULL) {
	interpreter(commands);
	}
	
}


