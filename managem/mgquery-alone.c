#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
typedef struct Gem_OB gem;    // the strange order is so that managem_utils knows which gem type are we defining as "gem"
#include "managem_utils.h"
#include "query_utils.h"
#include "gfon.h"
#include "print_utils.h"

void worker(int len, int output_options, int pool_zero, char* filename)
{
	FILE* table=file_check(filename);      // file is open to read
	if (table==NULL) exit(1);              // if the file is not good we exit
	int i;
	gem* gems=malloc(len*sizeof(gem));     // if not malloc-ed 230k is the limit
	gem** pool=malloc(len*sizeof(gem*));   // if not malloc-ed 200k is the limit (win)
	int* pool_length=malloc(len*sizeof(int));
	pool[0]=malloc(pool_zero*sizeof(gem));
	pool_length[0]=pool_zero;
	
	if (pool_zero==1) {                    // combine
		gem_init(pool[0],1,1,1);
		gem_init(gems   ,1,1,1);
	}
	else {                                 // spec
		gem_init(pool[0]  ,1,1,0);
		gem_init(pool[0]+1,1,0,1);
		gem_init(gems     ,1,1,0);
	}
	
	int prevmax=pool_from_table(pool, pool_length, len, table);    // pool filling
	fclose(table);				// close
	if (prevmax<len-1) {
		for (i=0;i<=prevmax;++i) free(pool[i]);      // free
		free(pool);				// free
		free(pool_length);	// free
		free(gems);				// free
		if (prevmax>0) printf("Table stops at %d, not %d\n",prevmax+1,len);
		exit(1);
	}

	int skip_computations = (output_options & mask_quiet) && !((output_options & mask_table) || (output_options & mask_upto));
	int first = skip_computations ? len-1 : 0;
	for (i=first; i<len; ++i) {
		gems[i]=pool[i][0];
		for (int j=1; j<pool_length[i]; ++j) {
			if (gem_more_powerful(pool[i][j],gems[i])) {
				gems[i]=pool[i][j];
			}
		}
		
		if (!(output_options & mask_quiet)) {
			printf("Value:\t%d\n",i+1);
			if (output_options & mask_info)
				printf("Growth:\t%f\n", log(gem_power(gems[i]))/log(i+1));
			if (output_options & mask_debug)
				printf("Pool:\t%d\n",pool_length[i]);
			gem_print(gems+i);
		}
	}
	
	if (output_options & mask_quiet) {     // outputs last if we never seen any
		printf("Value:\t%d\n",len);
		printf("Growth:\t%f\n", log(gem_power(gems[len-1]))/log(len));
		if (output_options & mask_debug)
			printf("Pool:\t%d\n",pool_length[len-1]);
		gem_print(gems+len-1);
	}

	gem* gemf=gems+len-1;  // gem that will be displayed

	if (output_options & mask_upto) {
		double best_growth=-INFINITY;
		int best_index=0;
		for (i=0; i<len; ++i) {
			if (log(gem_power(gems[i]))/log(i+1) > best_growth) {
				best_index=i;
				best_growth=log(gem_power(gems[i]))/log(i+1);
			}
		}
		printf("Best gem up to %d:\n\n", len);
		printf("Value:\t%d\n",best_index+1);
		printf("Growth:\t%f\n", best_growth);
		gem_print(gems+best_index);
		gemf = gems+best_index;
	}

	gem* gem_array = NULL;
	gem red;
	if (output_options & mask_red) {
		if (len < 3 || pool_zero!=2) printf("I could not add red!\n\n");
		else {
			int value=gem_getvalue(gemf);
			gemf = gem_putred(pool[value-1], pool_length[value-1], value, &red, &gem_array, 0);
			printf("Gem with red added:\n\n");
			printf("Value:\t%d\n", value);    // made to work well with -u
			printf("Growth:\t%f\n", log(gem_power(*gemf))/log(value));
			gem_print(gemf);
		}
	}

	if (output_options & mask_parens) {
		printf("Compressed combining scheme:\n");
		print_parens_compressed(gemf);
		printf("\n\n");
	}
	if (output_options & mask_tree) {
		printf("Gem tree:\n");
		print_tree(gemf, "");
		printf("\n");
	}
	if (output_options & mask_table) print_table(gems, len);
	
	if (output_options & mask_equations) {   // it ruins gems, must be last
		printf("Equations:\n");
		print_equations(gemf);
		printf("\n");
	}
	
	for (i=0;i<len;++i) free(pool[i]);     // free
	free(pool);    // free
	free(pool_length);
	free(gems);
	if (output_options & mask_red && len > 2 && pool_zero==2) {
		free(gem_array);
	}
}

int main(int argc, char** argv)
{
	int len;
	char opt;
	int pool_zero=2;     // speccing by default
	int output_options=0;
	char filename[256]="";     // it should be enough

	while ((opt=getopt(argc,argv,"hptecidqurf:"))!=-1) {
		switch(opt) {
			case 'h':
				print_help("hptecidqurf:");
				return 0;
			PTECIDQUR_OPTIONS_BLOCK
			case 'f':
				strcpy(filename,optarg);
				break;
			case '?':
				return 1;
			default:
				break;
		}
	}
	if (optind==argc) {
		printf("No length specified\n");
		return 1;
	}
	if (optind+1==argc) {
		len = atoi(argv[optind]);
		char* p=argv[optind];
		while (*p != '\0') p++;
		if (*(p-1)=='c') pool_zero=1;
	}
	else {
		printf("Too many arguments:\n");
		while (argv[optind]!=NULL) {
			printf("%s ", argv[optind]);
			optind++;
		}
		printf("\n");
		return 1;
	}
	if (len<1) {
		printf("Improper gem number\n");
		return 1;
	}
	if (pool_zero==2) file_selection(filename, "table_mgspec");
	else file_selection(filename, "table_mgcomb");
	worker(len, output_options, pool_zero, filename);
	return 0;
}

