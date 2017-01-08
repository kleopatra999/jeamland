# A rather silly sed program which does (almost) the same as
# mkcmdprot.l and mkcmdprot.awk
# but using sed.
# Difference: this one doesn't remove duplicates.. Could be done by piping
#             through sort, then uniq - but then the output wouldn't look
#             as nice ;-)
# Should be run using:
#	sed -n -f mkcmdprot.sed cmd.table.c > cmd.table.h

1	{	i\
/* cmd.table.h - prototypes for cmd.table.c
		i\
 * WARNING: This is an automatically generated file.
		i\
 *          any changes you make to it will be lost.
		i\
 */
		i\

}

/*.*commands */	{	i\

			p
}

/^#if.*$/ {		p
}

/^#end.*$/ {		p
}

/f_[a-z_]*/ {	
		s/^.*\(f_[a-z_]*\).*$/extern void \1(struct user *, int, char **);/1
		p
}

