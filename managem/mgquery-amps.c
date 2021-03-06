#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <string.h>
typedef struct Gem_OB gem;		// the strange order is so that managem_utils knows which gem type are we defining as "gem"
#include "managem_utils.h"
typedef struct Gem_O gemO;
#include "leech_utils.h"
#include "mga_utils.h"
#include "query_utils.h"
#include "gfon.h"
#include "print_utils.h"

void print_amps_table(gem* gems, gemO* amps, double* spec_coeffs, double leech_ratio, int len)
{
	printf("Managem\tAmps\tPower\t\tSpec coeff\n");
	int i;
	for (i=0; i<len; i++)
		printf("%d\t%d\t%#.7g\t%f\n", i+1, gem_getvalue_O(amps+i), gem_amp_power(gems[i], amps[i], leech_ratio), spec_coeffs[i]);
	printf("\n");
}

void worker(int len, int output_options, double growth_comb, char* filename, char* filenameA, int TC, int As, int Namps)
{
	FILE* table=file_check(filename);			// file is open to read
	if (table==NULL) exit(1);						// if the file is not good we exit
	int i;
	gem* pool[len];
	int pool_length[len];
	pool[0]=malloc(2*sizeof(gem));
	pool_length[0]=2;
	gem_init(pool[0]  ,1,1,0);
	gem_init(pool[0]+1,1,0,1);
	
	int prevmax=pool_from_table(pool, pool_length, len, table);		// managem pool filling
	fclose(table);
	if (prevmax<len-1) {										// if the managems are not enough
		for (i=0;i<=prevmax;++i) free(pool[i]);		// free
		if (prevmax>0) printf("Gem table stops at %d, not %d\n",prevmax+1,len);
		exit(1);
	}
	
	gem* poolf[len];
	int poolf_length[len];
	
	MGSPEC_COMPRESSION
	if (!(output_options & mask_quiet)) printf("Gem speccing pool compression done!\n");

	FILE* tableA=file_check(filenameA);		// fileA is open to read
	if (tableA==NULL) exit(1);					// if the file is not good we exit
	int lena=len;
	gemO* poolO[lena];
	int poolO_length[lena];
	poolO[0]=malloc(sizeof(gemO));
	poolO_length[0]=1;
	gem_init_O(poolO[0],1,1);
	
	int prevmaxA=pool_from_table_O(poolO, poolO_length, lena, tableA);		// amps pool filling
	fclose(tableA);
	if (prevmaxA<lena-1) {
		for (i=0;i<=prevmaxA;++i) free(poolO[i]);		// free
		if (prevmaxA>0) printf("Amp table stops at %d, not %d\n",prevmaxA+1,lena);
		exit(1);
	}

	gemO* bestO=malloc(lena*sizeof(gemO));		// if not malloc-ed 140k is the limit
	
	AMPS_COMPRESSION
	if (!(output_options & mask_quiet)) printf("Amp pool compression done!\n\n");

	int j,k;									// let's choose the right gem-amp combo
	gem gems[len];
	gemO amps[len];
	double spec_coeffs[len];
	gem_init(gems,1,1,0);
	amps[0]=(gemO){0};
	spec_coeffs[0]=0;
	double leech_ratio=Namps*(0.15+As/3*0.004)*2*(1+0.03*TC)/(1+TC/3*0.1);
	
	int skip_computations = (output_options & mask_quiet) && !((output_options & mask_table) || (output_options & mask_upto));
	int first = skip_computations ? len-1 : 0;
	for (i=first; i<len; ++i) {								// for every gem value
		gems[i]=(gem){0};									// we init the gems
		amps[i]=(gemO){0};									// to extremely weak ones
		for (k=0;k<poolf_length[i];++k) {					// first we compare the gem alone
			if (gem_power(poolf[i][k]) > gem_power(gems[i])) {
				gems[i]=poolf[i][k];
			}
		}
		int NS=i+1;
		double comb_coeff=pow(NS, -growth_comb);
		spec_coeffs[i]=comb_coeff*gem_power(gems[i]);
															// now with amps
		for (j=0, NS+=Namps; j<i+1; ++j, NS+=Namps) {		// for every amp value from 1 to to gem_value
			double comb_coeff=pow(NS, -growth_comb);		// we compute comb_coeff
			double Pa= leech_ratio * bestO[j].leech;		// <- this is ok only for mg
			for (k=0; k<poolf_length[i]; ++k) {				// then we search in the reduced gem pool
				double Palone = gem_power(poolf[i][k]);
				double power = Palone + poolf[i][k].bbound * Pa;
				double spec_coeff=power*comb_coeff;
				if (spec_coeff>spec_coeffs[i]) {
					spec_coeffs[i]=spec_coeff;
					gems[i]=poolf[i][k];
					amps[i]=bestO[j];
				}
			}
		}
		if (!(output_options & mask_quiet)) {
			printf("Total value:\t%d\n\n", i+1+Namps*gem_getvalue_O(amps+i));
			printf("Managem\n");
			printf("Value:\t%d\n",i+1);
			if (output_options & mask_debug) printf("Pool:\t%d\n",poolf_length[i]);
			gem_print(gems+i);
			printf("Amplifier (x%d)\n", Namps);
			printf("Value:\t%d\n",gem_getvalue_O(amps+i));
			gem_print_O(amps+i);
			printf("Spec base power: \t%#.7g\n", gem_amp_power(gems[i], amps[i], leech_ratio));
			printf("Spec coefficient:\t%f\n\n", spec_coeffs[i]);
		}
	}
	
	if (output_options & mask_quiet) {		// outputs last if we never seen any
		printf("Total value:\t%d\n\n", len+Namps*gem_getvalue_O(amps+len-1));
		printf("Managem\n");
		printf("Value:\t%d\n", len);
		gem_print(gems+len-1);
		printf("Amplifier (x%d)\n", Namps);
		printf("Value:\t%d\n", gem_getvalue_O(amps+len-1));
		gem_print_O(amps+len-1);
		printf("Spec base power: \t%#.7g\n", gem_amp_power(gems[len-1], amps[len-1], leech_ratio));
		printf("Spec coefficient:\t%f\n\n", spec_coeffs[len-1]);
	}

	gem*  gemf=gems+len-1;  // gem that will be displayed
	gemO* ampf=amps+len-1;  // amp that will be displayed

	if (output_options & mask_upto) {
		double best_sc=0;
		int best_index=0;
		for (i=0; i<len; ++i) {
			if (spec_coeffs[i] > best_sc) {
				best_index=i;
				best_sc=spec_coeffs[i];
			}
		}
		printf("Best setup up to %d:\n\n", len);
		printf("Total value:\t%d\n\n", gem_getvalue(gems+best_index)+Namps*gem_getvalue_O(amps+best_index));
		printf("Managem\n");
		printf("Value:\t%d\n", gem_getvalue(gems+best_index));
		gem_print(gems+best_index);
		printf("Amplifier (x%d)\n", Namps);
		printf("Value:\t%d\n", gem_getvalue_O(amps+best_index));
		gem_print_O(amps+best_index);
		printf("Spec base power: \t%#.7g\n", gem_amp_power(gems[best_index], amps[best_index], leech_ratio));
		printf("Spec coefficient:\t%f\n\n", best_sc);
		gemf = gems+best_index;
		ampf = amps+best_index;
	}

	gem* gem_array = NULL;
	gem red;
	if (output_options & mask_red) {
		if (len < 3) printf("I could not add red!\n\n");
		else {
			int value = gem_getvalue(gemf);
			int valueA= gem_getvalue_O(ampf);
			double NS = value + Namps*valueA;
			double amp_leech_scaled = leech_ratio * ampf->leech;
			gemf = gem_putred(poolf[value-1], poolf_length[value-1], value, &red, &gem_array, amp_leech_scaled);
			printf("Setup with red added:\n\n");
			printf("Total value:\t%d\n\n", value+Namps*gem_getvalue_O(ampf));
			printf("Managem\n");
			printf("Value:\t%d\n", value);
			gem_print(gemf);
			printf("Amplifier (x%d)\n", Namps);
			printf("Value:\t%d\n", gem_getvalue_O(ampf));
			gem_print_O(ampf);
			printf("Spec base power w. red:\t%#.7g\n", gem_amp_power(*gemf, *ampf, leech_ratio));
			double CgP = pow(NS, -growth_comb);
			printf("Spec coefficient:\t%f\n\n", CgP*gem_cfr_power(*gemf, amp_leech_scaled));
		}
	}

	if (output_options & mask_parens) {
		printf("Managem speccing scheme:\n");
		print_parens_compressed(gemf);
		printf("\n\n");
		printf("Amplifier speccing scheme:\n");
		print_parens_compressed_O(ampf);
		printf("\n\n");
	}
	if (output_options & mask_tree) {
		printf("Managem tree:\n");
		print_tree(gemf, "");
		printf("\n");
		printf("Amplifier tree:\n");
		print_tree_O(ampf, "");
		printf("\n");
	}
	if (output_options & mask_table) print_amps_table(gems, amps, spec_coeffs, leech_ratio, len);
	
	if (output_options & mask_equations) {		// it ruins gems, must be last
		printf("Managem equations:\n");
		print_equations(gemf);
		printf("\n");
		printf("Amplifier equations:\n");
		print_equations_O(ampf);
		printf("\n");
	}
	
	for (i=0;i<len;++i) free(pool[i]);			// free gems
	for (i=0;i<len;++i) free(poolf[i]);			// free gems compressed
	for (i=0;i<lena;++i) free(poolO[i]);		// free amps
	free(bestO);										// free amps compressed
	if (output_options & mask_red && len > 2) {
		free(gem_array);
	}
}

int main(int argc, char** argv)
{
	int len;
	char opt;
	int TC=120;
	int As=60;
	int Namps=6;
	double growth_comb=0.627216;		// 16c
	int output_options=0;
	char filename[256]="";		// it should be enough
	char filenameA[256]="";		// it should be enough

	while ((opt=getopt(argc,argv,"hptecdqurf:g:T:A:N:"))!=-1) {
		switch(opt) {
			case 'h':
				print_help("hptecdqurf:g:T:A:N:");
				return 0;
			PTECIDQUR_OPTIONS_BLOCK
			case 'f':		// can be "filename,filenameA", if missing default is used
				table_selection2(optarg, filename, filenameA);
				break;
			case 'g':
				growth_comb = atof(optarg);
				break;
			TAN_OPTIONS_BLOCK
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
	file_selection(filename, "table_mgspec");
	file_selection(filenameA, "table_leech");
	worker(len, output_options, growth_comb, filename, filenameA, TC, As, Namps);
	return 0;
}

