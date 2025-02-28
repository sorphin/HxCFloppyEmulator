/*
//
// Copyright (C) 2006-2023 Jean-Fran�ois DEL NERO
//
// This file is part of the HxCFloppyEmulator library
//
// HxCFloppyEmulator may be used and distributed without restriction provided
// that this copyright statement is not removed from the file and that any
// derivative work contains the original copyright notice and the associated
// disclaimer.
//
// HxCFloppyEmulator is free software; you can redistribute it
// and/or modify  it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// HxCFloppyEmulator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//   See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with HxCFloppyEmulator; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
*/
///////////////////////////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------------//
//-------------------------------------------------------------------------------//
//-----------H----H--X----X-----CCCCC----22222----0000-----0000------11----------//
//----------H----H----X-X-----C--------------2---0----0---0----0--1--1-----------//
//---------HHHHHH-----X------C----------22222---0----0---0----0-----1------------//
//--------H----H----X--X----C----------2-------0----0---0----0-----1-------------//
//-------H----H---X-----X---CCCCC-----222222----0000-----0000----1111------------//
//-------------------------------------------------------------------------------//
//----------------------------------------------------- http://hxc2001.free.fr --//
///////////////////////////////////////////////////////////////////////////////////
// File : krz_loader.c
// Contains: Kurzweil KRZ floppy image loader
//
// Written by:	DEL NERO Jean Francois
//
// Change History (most recent first):
///////////////////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "types.h"

#include "internal_libhxcfe.h"
#include "libhxcfe.h"
#include "libhxcadaptor.h"
#include "floppy_loader.h"
#include "tracks/track_generator.h"

#include "loaders/common/raw_iso.h"

#include "krz_loader.h"

#include "../fat12floppy_loader/fat12.h"

extern unsigned char msdos_bootsector[];

int KRZ_libIsValidDiskFile( HXCFE_IMGLDR * imgldr_ctx, HXCFE_IMGLDR_FILEINFOS * imgfile )
{
	return hxcfe_imgCheckFileCompatibility( imgldr_ctx, imgfile, "KRZ_libIsValidDiskFile", "krz", 0 );
}



int KRZ_libLoad_DiskFile(HXCFE_IMGLDR * imgldr_ctx,HXCFE_FLOPPY * floppydisk,char * imgfile,void * parameters)
{
	raw_iso_cfg rawcfg;
	int ret;
	unsigned char * flatimg;
	int numberofcluster;
	uint32_t fatposition;
	uint32_t rootposition;
	uint32_t dataposition;
	int pcbootsector;
	int dksize;
	FATCONFIG fatconfig;

	imgldr_ctx->hxcfe->hxc_printf(MSG_DEBUG,"krz_libLoad_DiskFile %s",imgfile);

	raw_iso_setdefcfg(&rawcfg);

	rawcfg.number_of_tracks = 80;
	rawcfg.number_of_sides = 2;
	rawcfg.number_of_sectors_per_track = 18;
	rawcfg.bitrate = 500000;
	rawcfg.rpm = 300;
	rawcfg.skew_per_track = 0;
	rawcfg.gap3 = 84;
	rawcfg.interleave = 1;
	rawcfg.sector_size = 512;
	rawcfg.track_format = IBMFORMAT_DD;
	rawcfg.interface_mode = IBMPC_HD_FLOPPYMODE;
	pcbootsector=1;

	dksize = floppydisk->floppyNumberOfTrack *
			(floppydisk->floppySectorPerTrack * floppydisk->floppyNumberOfSide * 512);

	flatimg=(unsigned char*)malloc(dksize);
	if(flatimg!=NULL)
	{
		memset(flatimg,0,dksize);
		if(pcbootsector)
			memcpy(flatimg,&msdos_bootsector,512);

		fatconfig.sectorsize = 512;
		fatconfig.clustersize = 2;
		fatconfig.reservedsector = 1;
		fatconfig.numberoffat = 2;
		fatconfig.numberofrootentries = 224;// 7secteurs = 32 *512
		fatconfig.nbofsector=(floppydisk->floppyNumberOfTrack*
								(floppydisk->floppySectorPerTrack*floppydisk->floppyNumberOfSide));
		fatconfig.nbofsectorperfat=((fatconfig.nbofsector-(fatconfig.reservedsector+(fatconfig.numberofrootentries/32)))/((fatconfig.sectorsize*8)/12))+1;
		//sprintf(&flatimg[CHSTOADR(0,0,0)+3],"HXC.EMU");

		*( (unsigned short*) &flatimg[0x0B])=fatconfig.sectorsize; //Nombre d'octets par secteur
		*( (unsigned char*)  &flatimg[0x0D])=fatconfig.clustersize; //Nombre de secteurs par cluster (1, 2, 4, 8, 16, 32, 64 ou 128).
		*( (unsigned short*) &flatimg[0x0E])=fatconfig.reservedsector; //Nombre de secteur r�serv� en comptant le secteur de boot (32 par d�faut pour FAT32, 1 par d�faut pour FAT12/16).
		*( (unsigned char*)  &flatimg[0x10])=fatconfig.numberoffat; //Nombre de FATs sur le disque (2 par d�faut).
		*( (unsigned short*) &flatimg[0x11])=fatconfig.numberofrootentries; //Taille du r�pertoire racine (0 par d�faut pour FAT32).
		*( (unsigned short*) &flatimg[0x13])=fatconfig.nbofsector; //Nombre total de secteur 16-bit (0 par d�faut pour FAT32).

		if(	floppydisk->floppyBitRate==250000)
		{
			*( (unsigned char*)  &flatimg[0x15])=0xF9; //Type de disque (0xF8 pour les disques durs, 0xF0 pour les disquettes). 0xF9 Double sided, 80 tracks per side, 9 sectors per track
		}
		else
		{
			*( (unsigned char*)  &flatimg[0x15])=0xF0; //Type de disque (0xF8 pour les disques durs, 0xF0 pour les disquettes). 0xF9 Double sided, 80 tracks per side, 9 sectors per track
		}

		*( (unsigned short*) &flatimg[0x16])=fatconfig.nbofsectorperfat; //Taille d'une FAT en secteurs (0 par d�faut pour FAT32).
		*( (unsigned short*) &flatimg[0x18])=floppydisk->floppySectorPerTrack; //Sectors per track
		*( (unsigned short*) &flatimg[0x1a])=floppydisk->floppyNumberOfSide; //Number of heads.
		*( (unsigned short*) &flatimg[0x1FE])=0xAA55;//End of sector marker (0x55 0xAA)

		fatposition = fatconfig.sectorsize*fatconfig.reservedsector;
		rootposition = ((fatconfig.reservedsector)+(fatconfig.numberoffat*fatconfig.nbofsectorperfat))*fatconfig.sectorsize;
		dataposition = (((fatconfig.reservedsector)+(fatconfig.numberoffat*fatconfig.nbofsectorperfat))*fatconfig.sectorsize)+(32*fatconfig.numberofrootentries);

		numberofcluster = (fatconfig.nbofsector-(dataposition/fatconfig.sectorsize))/fatconfig.clustersize;

		if(ScanFileAndAddToFAT(imgldr_ctx->hxcfe,imgfile,0,&flatimg[fatposition],&flatimg[rootposition],&flatimg[dataposition],0,&fatconfig,numberofcluster))
		{
			return HXCFE_BADFILE;
		}

		memcpy(&flatimg[((fatconfig.reservedsector)+(fatconfig.nbofsectorperfat))*fatconfig.sectorsize],&flatimg[fatposition],fatconfig.nbofsectorperfat*fatconfig.sectorsize);

		ret = raw_iso_loader(imgldr_ctx, floppydisk, 0, flatimg, dksize, &rawcfg);

		free(flatimg);

		return ret;

	}
	else
	{
		return HXCFE_INTERNALERROR;
	}
}

int KRZ_libGetPluginInfo(HXCFE_IMGLDR * imgldr_ctx,uint32_t infotype,void * returnvalue)
{

	static const char plug_id[]="KURZWEIL_KRZ";
	static const char plug_desc[]="KURZWEIL KRZ Loader";
	static const char plug_ext[]="krz";

	plugins_ptr plug_funcs=
	{
		(ISVALIDDISKFILE)	KRZ_libIsValidDiskFile,
		(LOADDISKFILE)		KRZ_libLoad_DiskFile,
		(WRITEDISKFILE)		0,
		(GETPLUGININFOS)	KRZ_libGetPluginInfo
	};

	return libGetPluginInfo(
			imgldr_ctx,
			infotype,
			returnvalue,
			plug_id,
			plug_desc,
			&plug_funcs,
			plug_ext
			);
}



