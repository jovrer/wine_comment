/*
 * Copyright (c) Michael Hipp and other authors of the mpglib project.
 *
 * Layer 2 Alloc tables ..
 * most other tables are calculated on program start (which is (of course)
 * not ISO-conform) ..
 * Layer-3 huffman table is in huffman.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

struct al_table
{
  short bits;
  short d;
};

struct al_table alloc_0[] = {
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767} };

struct al_table alloc_1[] = {
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{3,-3},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},{10,-511},
	{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
	{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{3,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767},
	{2,0},{5,3},{7,5},{16,-32767} };

struct al_table alloc_2[] = {
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63} };

struct al_table alloc_3[] = {
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{4,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},{9,-255},
	{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},{15,-16383},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63} };

struct al_table alloc_4[] = {
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{4,0},{5,3},{7,5},{3,-3},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},{8,-127},
		{9,-255},{10,-511},{11,-1023},{12,-2047},{13,-4095},{14,-8191},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{3,0},{5,3},{7,5},{10,9},{4,-7},{5,-15},{6,-31},{7,-63},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
	{2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9},
    {2,0},{5,3},{7,5},{10,9}  };
