// ----------------------------------------------------------------- //
//																	 //
//		Virtual Memory Manager Simulator							 //
//																	 //
//		60-330 Final Project - Project for Two						 //
//		Kevin Ng 													 //
//		103155072													 //
//																	 //
// ----------------------------------------------------------------- //

// ----------------------- #INCLUDE HEADERS ------------------------ //

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

// ------------------------- #DEFINITIONS -------------------------- //

#define PAGE_SIZE_BYTES 256
#define FRAME_SIZE_BYTES 256
#define NUM_PAGES 256
#define NUM_FRAMES 256
#define TLB_ENTRIES 16
#define TLB_COLS 2

// ----------------------- #GLOBAL VARIABLES ----------------------- //

int page_table[NUM_PAGES];
int phys_mem[NUM_FRAMES][FRAME_SIZE_BYTES];
int tlb[TLB_ENTRIES][TLB_COLS];
int curr_free_frame = 0;

int page_faults = 0;
int addr_translated = 0;
int tlb_hit = 0;
int tlb_occu_slots = 0;

int correct = 0;
int at_line = 0;

// -------------------------- #FUNCTIONS --------------------------- //

void init_page_table()
{
	for (int i = 0; i < NUM_PAGES; ++i)
		page_table[i] = -1;
}

void init_tlb()
{
	for (int i = 0; i < TLB_ENTRIES; ++i)
	{
		for (int j = 0; j < TLB_COLS; ++j)
			tlb[i][j] = -1;
	}
}

void demand_page(int pg_num)
{
	char buffer[PAGE_SIZE_BYTES];
	FILE * fp_bs;

	if ( (fp_bs = fopen("BACKING_STORE.bin", "rb")) == NULL )
	{
		fprintf(stderr, "Missing BACKING_STORE.bin\n");
		exit(EXIT_FAILURE);
	}

	if ( fseek(fp_bs, pg_num * PAGE_SIZE_BYTES, SEEK_SET) != 0 )
	{
		fprintf(stderr, "Seek failure\n");
		exit(EXIT_FAILURE);
	}

	// read page from backing store into buffer
	if ( fread(buffer, 1, PAGE_SIZE_BYTES, fp_bs) != PAGE_SIZE_BYTES )	
	{
		fprintf(stderr, "Read failure\n");
		exit(EXIT_FAILURE);
	}

	fclose(fp_bs);

	// transfer buffered page to free frame of physical memory
	for (int i = 0; i < PAGE_SIZE_BYTES; ++i)
		phys_mem[curr_free_frame][i] = buffer[i];
}

int page_table_lookup(int pg_num, int offset)
{
	if (page_table[pg_num] == -1)
	{
		// store current frame in page table
		page_table[pg_num] = curr_free_frame;

		// bring page from backing store into current frame of phys mem
		demand_page(pg_num);

		++page_faults;
		++curr_free_frame;
	} 

	return phys_mem[page_table[pg_num]][offset];
}

void update_tlb(int pg_num)
{
	if (tlb_occu_slots < TLB_ENTRIES)
	{
		for (int i = 0; i < TLB_ENTRIES; ++i)
		{
			if (tlb[i][0] == -1)
			{
				tlb[i][0] = pg_num;
				tlb[i][1] = page_table[pg_num];
				++tlb_occu_slots;
				break;
			}
		}
	}

	// tlb full, update using FIFO
	if (tlb_occu_slots >= TLB_ENTRIES)
	{
		for (int i = 0; i < TLB_ENTRIES - 1; ++i)
		{
			tlb[i][0] = tlb[i+1][0];
			tlb[i][1] = tlb[i+1][1];
		}

		// put new entry in last position
		tlb[TLB_ENTRIES-1][0] = pg_num;
		tlb[TLB_ENTRIES-1][1] = page_table[pg_num];
	}
}

int tlb_lookup(int pg_num, int offset)
{
	int value, tlb_miss = 1; 

	for (int i = 0; i < TLB_ENTRIES; ++i)
	{
		if (tlb[i][0] == pg_num)
		{
			// successful tlb look-up. Get mem value directly.
			int frame = tlb[i][1];
			value = phys_mem[frame][offset];
			tlb_miss = 0;
			++tlb_hit;
			break;
		}
	}

	if (tlb_miss)
	{
		// tlb miss, try page table, then update tlb entry 
		value = page_table_lookup(pg_num, offset);
		update_tlb(pg_num);
	}

	return value;
}

#ifdef _ERROR_CHECK
int match(int virt_addr, int phys_addr, int value)
{
	FILE * fp_correct;

	if ( (fp_correct = fopen("correct.txt", "r")) == NULL)
	{
		fprintf(stderr, "Cannot open correct.txt\n");
		exit(EXIT_FAILURE);
	}

	int matched = 0;
	char line[64];
	char * tokens;  
	
	errno = 0;

	for (int i = 0; i < at_line+1; ++i)
	{
		fgets(line, sizeof line, fp_correct);

		// only parse the line corresponding to the one being looked-up,
		if (i == at_line) {
			char * endptr;
			char * token = strtok(line, " ");
			int parsed_int;

			while (token != NULL)
			{
				parsed_int = strtol(token, &endptr, 10);

				if (endptr > token) // make sure entire number is grabbed by strtol
				{
					if ((errno == ERANGE && (parsed_int == LONG_MAX || parsed_int == LONG_MIN))
	            			|| (errno != 0 && parsed_int == 0)) {
						fprintf(stderr, "strtol conversion error %d\n", errno);
	        			exit(EXIT_FAILURE);
	    			} 
	    			else {
	    				if (parsed_int == virt_addr || parsed_int == phys_addr
	    						|| parsed_int == value)
	    					++matched;
	    			}
	    		}
				token = strtok(NULL, " ");
			}
		}
	}

	fclose(fp_correct);

	return matched == 3 ? 1 : 0;
}
#endif

// ----------------------------------------------------------------- //

int main(int argc, char * argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <address file>\n", argv[0]);
		return -1;
	}

	init_page_table();
	init_tlb();

	FILE * fp_addr;
	int virt_addr, phys_addr, pagenum, offset, value;

	if ( (fp_addr = fopen(argv[1], "r")) == NULL )
	{
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		return -1;
	}

	#ifdef _ERROR_CHECK
		printf("%7s  %8s  %5s  %5s\n", "VIRTUAL", "PHYSICAL", "VALUE", "MATCH");
	#endif

	while( fscanf(fp_addr, "%d", &virt_addr) == 1 )
	{
		pagenum = (virt_addr & 0xff00) >> 8;
		offset = virt_addr & 0xff;

		#if 0
			value = page_table_lookup(pagenum, offset);	// page table only implementation
		#else 
			value = tlb_lookup(pagenum, offset);
		#endif

		phys_addr = page_table[pagenum] * PAGE_SIZE_BYTES + offset;
		++addr_translated;

		#ifdef _ERROR_CHECK
		// check if the values obtained match the correct outputs file
			char * match_str = "---";
			if (match(virt_addr, phys_addr, value))
			{
				match_str = "\xE2\x9C\x93";
				++correct;
			}
			++at_line;

			printf("%7d  %8d  %5d  %5s\n", virt_addr, phys_addr, value, match_str);
		#else 
			printf("Virtual address: %i  Physical address: %i Value: %i\n", virt_addr, phys_addr, value);
		#endif
	}

	printf("\nAddresses Translated = %d\n", addr_translated);
	printf("Page Faults = %d\n", page_faults);
	printf("Page Fault Rate = %.3f\n", (double)page_faults / (double)addr_translated);
	printf("TLB Hits = %d\n", tlb_hit);
	printf("TLB Hit Rate = %.3f\n", (double)tlb_hit / (double)addr_translated);
	#ifdef _ERROR_CHECK 
		printf("Matches to correct.txt = %d\n", correct);
	#endif

	fclose(fp_addr);

	return 0;
}

