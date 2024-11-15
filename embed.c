/*
 * Usage: embed variable-name file
 *
 * Embeds the contents of the file as variable-name. It creates two variables
 * begin and end, so in a C source file you can load the data as follows:
 *
 *     extern char foo_begin[];
 *     extern char foo_end[];
 */
#include <elf.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct string {
	char *at;
	size_t length;
} str;

/* Reads an entire file into memory */
static str
read_file(const char *filename)
{
	str result = {0};
	FILE *file = fopen(filename, "rb");
	if (file) {
		fseek(file, 0, SEEK_END);
		result.length = ftell(file);
		fseek(file, 0, SEEK_SET);

		result.at = malloc(result.length + 1);
		if (result.at) {
			fread(result.at, result.length, 1, file);
			result.at[result.length] = '\0';
		}

		fclose(file);
	}

	return result;
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Not enough arguments\n");
		return 1;
	}

	/*
	 * The first argument is the name of the created variable. The program will
	 * create two variables for the beginning and end of the data respectively.
	 */
	char *name = argv[1];

	/*
	 * The second argument is the path to the file.
	 */
	char *input = argv[2];
	str data = read_file(input);

	/*
	 * The input file path is used for the output, too. The file extension is
	 * stripped from the path and we append the .o extension for the object
	 * file.
	 */
	char *end = input + strlen(input);
	while (end-- > input) {
		if (*end == '.') {
			goto next;
		} else if (*end == '/') {
			/* No file extension; we're at the file separator */
			end = input + strlen(input);
			goto next;
		}
	}

	end = input + strlen(input);
next:
	/* Create a new string for the output file */
	char *output = malloc(end - input + 2);
	/* Append the filename without the extension */
	memcpy(output, input, end - input);
	/* Append the new file extension */
	memcpy(output + (end - input), ".o", 2);

	FILE *f = fopen(output, "w");
	if (f == NULL) {
		perror(output);
		return 1;
	}

	Elf64_Ehdr h = {0};
	h.e_ident[EI_MAG0] = ELFMAG0;
	h.e_ident[EI_MAG1] = ELFMAG1;
	h.e_ident[EI_MAG2] = ELFMAG2;
	h.e_ident[EI_MAG3] = ELFMAG3;
	h.e_ident[EI_CLASS] = ELFCLASS64;
	h.e_ident[EI_DATA] = ELFDATA2LSB;
	h.e_ident[EI_VERSION] = EV_CURRENT;
	h.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	h.e_ident[EI_ABIVERSION] = 0;
	h.e_ident[EI_NIDENT] = sizeof(h.e_ident);
	h.e_type = ET_REL;
	h.e_machine = EM_X86_64;
	h.e_version = EV_CURRENT;
	h.e_shoff = sizeof(h);
	h.e_shnum = 5;
	h.e_shentsize = sizeof(Elf64_Shdr);
	h.e_shstrndx = 1;
	fwrite(&h, sizeof(h), 1, f);

	char shstrs[] = "\0.shstrtab\0.strtab\0.symtab\0.rodata";

	// write the section headers

	/* The first section in an ELF file must be the null section. */
	Elf64_Shdr null_section = {0};
	fwrite(&null_section, sizeof(null_section), 1, f);

	/*
	 * The next section contains the names of the sections. We store the names
	 * as a sequence of null-terminated strings in the shstrs.
	 */
	Elf64_Shdr shstrtab = {0};
	shstrtab.sh_name = 1;
	shstrtab.sh_type = SHT_STRTAB;
	shstrtab.sh_offset = sizeof(h) + h.e_shnum * sizeof(Elf64_Shdr);
	shstrtab.sh_size = sizeof(shstrs);
	fwrite(&shstrtab, sizeof(shstrtab), 1, f);

	Elf64_Shdr strtab = {0};
	strtab.sh_name = 11;
	strtab.sh_type = SHT_STRTAB;
	strtab.sh_offset = shstrtab.sh_offset + shstrtab.sh_size;
	/* Use a crude calculation for the size of the string table */
	strtab.sh_size = 2 * strlen(name) + sizeof("\0_begin\0_end\0.rodata");
	fwrite(&strtab, sizeof(strtab), 1, f);

	Elf64_Shdr symtab = {0};
	symtab.sh_name = 19;
	symtab.sh_type = SHT_SYMTAB;
	symtab.sh_link = 2;
	symtab.sh_info = 2;

	/*
	 * There are four symbols in total: the null symbol, the section symbol
	 * and the begin and end symbols
	 */
	symtab.sh_size = 4 * sizeof(Elf64_Sym);
	symtab.sh_entsize = sizeof(Elf64_Sym);
	symtab.sh_offset = strtab.sh_offset + strtab.sh_size;
	fwrite(&symtab, sizeof(symtab), 1, f);

	Elf64_Shdr rodata = {0};
	rodata.sh_name = 27;
	rodata.sh_type = SHT_PROGBITS;
	rodata.sh_flags = SHF_ALLOC;
	rodata.sh_offset = symtab.sh_offset + symtab.sh_size;
	rodata.sh_size = data.length;
	fwrite(&rodata, sizeof(rodata), 1, f);

	// .shstrtab data
	fwrite(shstrs, 1, sizeof(shstrs), f);

	// .strtab data
	fputc('\0', f);
	fputs(name, f);
	fputs("_begin", f);
	fputc('\0', f);
	fputs(name, f);
	fputs("_end", f);
	fputc('\0', f);
	fputs(".rodata", f);
	fputc('\0', f);

	// .symtab data
	Elf64_Sym sym = {0};

	// null symbol
	fwrite(&sym, sizeof(sym), 1, f);

	// section symbol
	sym.st_name = 1 + strlen(name) + sizeof("_begin\0_end");
	sym.st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
	sym.st_shndx = 4;
	sym.st_size = 0;
	fwrite(&sym, sizeof(sym), 1, f);

	// begin symbol
	memset(&sym, 0, sizeof(sym));
	sym.st_name = 1;
	sym.st_info = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
	sym.st_shndx = 4;
	sym.st_size = 8;
	fwrite(&sym, sizeof(sym), 1, f);

	// end symbol
	memset(&sym, 0, sizeof(sym));
	sym.st_name = 1 + strlen(name) + sizeof("_begin");
	sym.st_info = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
	sym.st_shndx = 4;
	sym.st_size = 8;
	sym.st_value = data.length;
	fwrite(&sym, sizeof(sym), 1, f);

	// .rodata data
	fwrite(data.at, 1, data.length, f);

	fclose(f);
	return 0;
}
