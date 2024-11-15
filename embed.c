#include <elf.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct string {
	char *at;
	size_t length;
} str;

static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(0);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

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

	// TODO: Use first command-line argument as name
	char *name = argv[1];
	char *input = argv[2];
	str data = read_file(input);

	// TODO: Choose a temporary filename

	char *end = input + strlen(input);
	while (end-- > input) {
		if (*end == '.') {
			goto next;
		} else if (*end == '/') {
			end = input + strlen(input);
			goto next;
		}
	}

	end = input + strlen(input);
next:
	char *output = malloc(end - input + 2);
	memcpy(output, input, end - input);
	memcpy(output + (end - input), ".o", 2);

	FILE *f = fopen(output, "w");
	if (f == NULL) {
		die("%s:", output);
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
	Elf64_Shdr null_section = {0};
	fwrite(&null_section, sizeof(null_section), 1, f);

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
	strtab.sh_size = strlen(name) + 2 + sizeof(".rodata");
	fwrite(&strtab, sizeof(strtab), 1, f);

	Elf64_Shdr symtab = {0};
	symtab.sh_name = 19;
	symtab.sh_type = SHT_SYMTAB;
	symtab.sh_link = 2;
	symtab.sh_info = 2;
	symtab.sh_size = 3 * sizeof(Elf64_Sym);
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
	fputc('\0', f);
	fputs(".rodata", f);
	fputc('\0', f);

	// .symtab data
	Elf64_Sym sym = {0};

	// null symbol
	fwrite(&sym, sizeof(sym), 1, f);

	// section symbol
	sym.st_name = strlen(name) + 2;
	sym.st_info = ELF64_ST_INFO(STB_LOCAL, STT_SECTION);
	sym.st_shndx = 4;
	sym.st_size = 0;
	fwrite(&sym, sizeof(sym), 1, f);

	// data symbol
	memset(&sym, 0, sizeof(sym));
	sym.st_name = 1;
	sym.st_info = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
	sym.st_shndx = 4;
	sym.st_size = 8;
	fwrite(&sym, sizeof(sym), 1, f);

	// .rodata data
	fwrite(data.at, 1, data.length, f);

	fclose(f);
	return 0;
}
