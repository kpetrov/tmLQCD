/***********************************************************************
* Copyright (C) 2002,2003,2004,2005,2006,2007,2008 Carsten Urbach
*
* This file is part of tmLQCD.
*
* tmLQCD is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* tmLQCD is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with tmLQCD.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/

#include "gauge.ih"

int read_binary_gauge_data_parallel(LemonReader * lemonreader, DML_Checksum * checksum)
{
  int t, x, y, z, status=0;
  int latticeSize[] = {L, L, L, T_global};
  int prec;
  DML_SiteRank rank;
  MPI_Offset bytes;
  uint64_t fbsu3;
  char *filebuffer, *current;

  bytes = lemonReaderBytes(lemonreader);

  if (bytes == g_nproc * VOLUME * 4 * sizeof(su3))
    prec = 64;
  else if (bytes == g_nproc * VOLUME * 4 * sizeof(su3) / 2)
    prec = 32;
  else
  {
    fprintf(stderr, "Probably wrong lattice size or precision (bytes=%lu).\n", (unsigned long)bytes);
    fprintf(stderr, "Panic! Aborting...\n");
    fflush( stdout );
    MPI_File_close(lemonreader->fh);
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
    exit(501);
  }

  if(g_cart_id == 0 && g_debug_level > 2)
    printf("# %d Bit precision read.\n", prec);

  DML_checksum_init(checksum);

  fbsu3 = sizeof(su3);
  if (prec == 32)
    fbsu3 /= 2;
  bytes = 4 * fbsu3;

  filebuffer = malloc(VOLUME * bytes);

  lemonReadLatticeParallel(lemonreader, filebuffer, bytes, latticeSize);

  if(status < 0 && status != LEMON_EOR)
  {
    fprintf(stderr, "LEMON read error occured with status = %d while reading!\nPanic! Aborting...\n", status);
    MPI_File_close(lemonreader->fh);
    MPI_Abort(MPI_COMM_WORLD, 1);
    MPI_Finalize();
    exit(500);
  }

  for(t = 0; t < T; t++)
    for(z = 0; z < LZ; z++)
      for(y = 0; y < LY; y++)
        for(x = 0; x < LX; x++)
        {
          rank = (DML_SiteRank) (g_proc_coords[1]*LX +
              (((g_proc_coords[0]*T+t)*g_nproc_z*LZ+g_proc_coords[3]*LZ+z)*g_nproc_y*LY
                + g_proc_coords[2]*LY+y)*((DML_SiteRank)LX*g_nproc_x) + x);
          current = filebuffer + bytes * (x + (y + (t * LZ + z) * LY) * LX);
          DML_checksum_accum(checksum, rank, current, bytes);
#ifndef WORDS_BIGENDIAN
          if(prec == 32)
          {
            byte_swap_assign_single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][1], current            , sizeof(su3)/8);
            byte_swap_assign_single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][2], current +     fbsu3, sizeof(su3)/8);
            byte_swap_assign_single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][3], current + 2 * fbsu3, sizeof(su3)/8);
            byte_swap_assign_single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][0], current + 3 * fbsu3, sizeof(su3)/8);
          }
          else
          {
            byte_swap_assign(&g_gauge_field[ g_ipt[t][x][y][z] ][1], current            , sizeof(su3)/8);
            byte_swap_assign(&g_gauge_field[ g_ipt[t][x][y][z] ][2], current +     fbsu3, sizeof(su3)/8);
            byte_swap_assign(&g_gauge_field[ g_ipt[t][x][y][z] ][3], current + 2 * fbsu3, sizeof(su3)/8);
            byte_swap_assign(&g_gauge_field[ g_ipt[t][x][y][z] ][0], current + 3 * fbsu3, sizeof(su3)/8);
          }
#else
          if(prec == 32)
          {
            single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][0], current            , sizeof(su3)/8);
            single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][1], current +     fbsu3, sizeof(su3)/8);
            single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][2], current + 2 * fbsu3, sizeof(su3)/8);
            single2double(&g_gauge_field[ g_ipt[t][x][y][z] ][3], current + 3 * fbsu3, sizeof(su3)/8);
          }
          else
          {
            memcpy(&g_gauge_field[ g_ipt[t][x][y][z] ][0], current            , sizeof(su3));
            memcpy(&g_gauge_field[ g_ipt[t][x][y][z] ][1], current +     fbsu3, sizeof(su3));
            memcpy(&g_gauge_field[ g_ipt[t][x][y][z] ][2], current + 2 * fbsu3, sizeof(su3));
            memcpy(&g_gauge_field[ g_ipt[t][x][y][z] ][3], current + 3 * fbsu3, sizeof(su3));
          }
#endif
        }
  DML_global_xor(&checksum->suma);
  DML_global_xor(&checksum->sumb);

  free(filebuffer);

  return(0);
}
