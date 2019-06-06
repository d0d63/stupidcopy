/*
 * $dodge: stupidcopy.c,v 1.7 2012/04/28 02:33:17 dodge Exp $
 * TO DO:
 *   - Report list of failed blocks
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/*
#define STUPID_BLOCKSIZE 8192
*/

int
num_blocks_written (size_t num_blocks, int *block_array)
{
	size_t	block_num,
		num_written;

	num_written = 0;

	for (block_num = 0; block_num < num_blocks; block_num++)
	{
		if (block_array[block_num])
		{
			num_written++;
		}
	}

	return num_written;
}

size_t
pick_a_block(size_t num_blocks, int *block_array)
{
	size_t	 misses,
		 this_one;
	FILE	*fd;

	fd = fopen("/dev/random", "r");
	if (fd == NULL)
	{
		fprintf(stderr, "Can't open /dev/random: %s\n", strerror(errno));
		exit(1);	/* XXX be more graceful */
	}

	fread(&this_one, sizeof(this_one), 1, fd);
	fclose(fd);

	this_one = this_one % num_blocks;
	for (misses = 0; misses < num_blocks; misses++)
	{
		if (block_array[this_one] == 0)
		{
			return this_one;
		}

		this_one = (this_one + 1) % num_blocks;
	}

	/* XXX be more graceful */
	fprintf(stderr, "It seems like all the blocks have been written.\n");
	exit(2);
}

int
main (int argc, char **argv)
{
	struct stat	 stat_buf;
	int		*block_array,
			 block_num,
			 d_dst,
			 d_src;
	size_t		 bytes_read,
			 bytes_written,
			 num_blocks;
	unsigned char	*payload;
	char		*filename_dst,
			*filename_src;
	off_t		 actual_offset;
	double		 num_blocks_f,
			 num_blocks_written_f;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <src> <dst>\n", argv[0]);
		return 1;
	}

	filename_dst = argv[2];
	filename_src = argv[1];

	if (stat(filename_src, &stat_buf))
	{
		fprintf(stderr, "Can't stat %s: %s\n", filename_src, strerror(errno));
		return 1;
	}

#ifdef STUPID_BLOCKSIZE
	stat_buf.st_blksize = STUPID_BLOCKSIZE;
#endif /* STUPID_BLOCKSIZE */

	num_blocks = (stat_buf.st_size / stat_buf.st_blksize);
	if (stat_buf.st_size % stat_buf.st_blksize)
	{
		num_blocks++;
	}

	printf("file size:%d -- block size:%d -- num_blocks:%d\n", (int)stat_buf.st_size, (int)stat_buf.st_blksize, (int)num_blocks);

	block_array = calloc(num_blocks, sizeof(*block_array));
	if (block_array == NULL)
	{
		fprintf(stderr, "calloc(%d, %d) failed: %s\n", (int)num_blocks, (int)sizeof(*block_array), strerror(errno));
		return 1;
	}

	payload = malloc(stat_buf.st_blksize);
	if (payload == NULL)
	{
		fprintf(stderr, "malloc(%d) for payload failed: %s\n", (int)stat_buf.st_blksize, strerror(errno));
		return 1;
	}

	d_src = open(filename_src, O_RDONLY | O_NONBLOCK);
	if (d_src == -1)
	{
		fprintf(stderr, "Can't open %s for reading: %s\n", filename_src, strerror(errno));
		return 1;
	}

	d_dst = open(filename_dst, O_RDWR | O_CREAT | O_TRUNC);
	if (d_dst == -1)
	{
		fprintf(stderr, "Can't open %s for writing: %s\n", filename_dst, strerror(errno));
		return 1;
	}
	fchmod(d_dst, stat_buf.st_mode);

	while (num_blocks_written(num_blocks, block_array) != num_blocks)
	{
		block_num = pick_a_block(num_blocks, block_array);

		actual_offset = lseek(d_src, block_num * stat_buf.st_blksize, SEEK_SET);
		if ((block_num * stat_buf.st_blksize) != actual_offset)
		{
			fprintf(stderr, "lseek tried to go to %d in src, but only went to %d\n", (int)(block_num * stat_buf.st_blksize), (int)actual_offset);
			return 1;
		}

		actual_offset = lseek(d_dst, block_num * stat_buf.st_blksize, SEEK_SET);
		if ((block_num * stat_buf.st_blksize) != actual_offset)
		{
			fprintf(stderr, "lseek tried to go to %d in dst, but only went to %d\n", (int)(block_num * stat_buf.st_blksize), (int)actual_offset);
			return 1;
		}

		bzero(payload, stat_buf.st_blksize);
		bytes_read = read(d_src, payload, stat_buf.st_blksize);
		if (		block_num != num_blocks - 1 &&
				(bytes_read < stat_buf.st_blksize ||
				 bytes_read == -1)
		   ) {
			fprintf(stderr, "Warning: block %d only read %d of %d bytes: %s\n", (int)block_num, (int)bytes_read, (int)stat_buf.st_blksize, strerror(errno));
		}

		bytes_written = write(d_dst, payload, bytes_read);
		num_blocks_f = num_blocks;
		num_blocks_written_f = 1 + num_blocks_written(num_blocks, block_array);
		printf("%d/%d -- Block %d done (%2.2f%%) -- read %d, wrote %d\n",
				1 + num_blocks_written(num_blocks, block_array),
				(int)num_blocks,
				block_num,
				100 * num_blocks_written_f / num_blocks_f,
				(int)bytes_read,
				(int)bytes_written);

		if (bytes_read != bytes_written)
		{
			fprintf(stderr, "Block %d read %d bytes, but only wrote %d bytes: %s.\n", block_num, (int)bytes_read, (int)bytes_written, strerror(errno));
			return 1;
		}

		block_array[block_num] = 1;
	}


	close(d_src);
	close(d_dst);
	return 0;
}
