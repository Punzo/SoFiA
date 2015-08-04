#!/usr/bin/python
# -*- coding: utf-8 -*-

# WCS coordinates
# GIPSY header repaired following this description:
# http://www.astro.rug.nl/software/kapteyn/spectralbackground.html#a-recipe-for-modification-of-nmap-gipsy-fits-data


def fix_gipsy_header(header_orig):
	
	import math
	import sys
	
	## gipsy keys for spectral axis
	key_opt = ['FREQ-OHEL','FREQ-OLSR']
	key_rad = ['FREQ-RHEL','FREQ-RLSR']
	c = 299792458.0  # speed of light in m/s
	header = header_orig.copy()
	naxis = header['NAXIS']
	
	for i in range(1,naxis+1):
		ctype = header['CTYPE%d'%i]
		if ctype in key_opt+key_rad:
			axis = i
			## read reference velocity - from VELR or DRVAL
			try:
				if 'VELR' in header:
					vel = header['VELR']
				elif 'DRVAL%d'%axis in header:
					vel = header['VELR']
					unit = header['DUNIT%d'%axis]
					if unit.lower() == 'km/s':
						vel = vel*1000.
					elif unit.lower()!='m/s':
						break
			except:
				sys.stderr.write("WARNING: Problem with reference velocity.\n")
				break
			
			## Convert reference frequency to Hz
			try:
				freq  = header['CRVAL%d'%axis]
				dfreq = header['CDELT%d'%axis]
				unit  = header['CUNIT%d'%axis]
				freqUnits = ['hz','khz','mhz','ghz']
				j = freqUnits.index(unit.lower())
				freq  *= 10**j
				dfreq *= 10**j
			except:
				sys.stderr.write("WARNING: Problem with reference frequency.\n")
				break
			
			## Need rest frequency for conversion
			try:
				freq0Names = ['FREQ0','FREQR','RESTFRQ']
				for key in freq0Names:
					try:
						freq0 = header[key]
						foundFreq0 = 1
					except:
						pass
				header['RESTFRQ'] = freq0
				#foundFreq0
			except:
				sys.stderr.write("WARNING: Rest frequency not found.\n")
				break
			
			## calculate reference frequency in the barycentric system
			if ctype in key_opt:
				freqB = freq0/(1.+vel/c)
			else:
				freqB = freq0/(1.-vel/c)
			## calculate topocentric velocity
			velT = c*((freqB**2-freq**2)/(freqB**2+freq**2))
			dfreqB = dfreq*math.sqrt((c-velT)/(c+velT))
			header['CTYPE%d'%axis] = 'FREQ'
			header['CUNIT%d'%axis] = 'Hz'
			header['CRVAL%d'%axis] = freqB
			header['CDELT%d'%axis] = dfreqB
			## GIPSY headers seem to contain the unit 'DEGREE' for RA/Dec
			## WCS lib does not like that
			for key in header:
				if 'CUNIT' in key and header[key] == 'DEGREE':
					header[key] = 'deg'
			print 'Header repaired successfully.'
			
			return header


def add_wcs_coordinates(objects,catParNames,catParFormt,catParUnits,Parameters):
	import imp
	import sys
	import numpy as np
	try:
	    imp.find_module('astropy')
	    found = True
	except ImportError: found = False
	
	if found:
		try:	
			from astropy import wcs
			from astropy.io import fits
			
			hdulist = fits.open(Parameters['import']['inFile'])
			header = hdulist[0].header
			hdulist.close()

			# Fix headers where "per second" is written "/S" instead of "/s"
			# (assuming they mean "per second" and not "per Siemens").
			if 'cunit3' in header and '/S' in header['cunit3']:
				print 'WARNING: Converting "/S" to "/s" in CUNIT3.'
				header['cunit3']=header['cunit3'].replace('/S','/s')
			
			## check if there is a Nmap/GIPSY FITS header keyword value present
			gipsyKey = [k for k in ['FREQ-OHEL','FREQ-OLSR','FREQ-RHEL','FREQ-RLSR'] if (k in [header[key] for key in header if ('CTYPE' in key)])]
			if gipsyKey:
				print 'GIPSY header found. Trying to convert it.'
				from astropy.wcs import Wcsprm
				header = fix_gipsy_header(header)
				wcsin = Wcsprm(str(header))
				wcsin.sptr('VOPT-F2W')
				if header['naxis']==4:
					objects = np.concatenate((objects,wcsin.p2s(np.concatenate((objects[:,catParNames.index('x'):catParNames.index('x')+3],np.zeros((objects.shape[0],1))),axis=1),0)['world'][:,:-1]),axis=1)
				else:
					objects = np.concatenate((objects,wcsin.p2s(objects[:,catParNames.index('x'):catParNames.index('x')+3],0)['world']),axis=1)
				catParUnits = tuple(list(catParUnits) + [str(cc).replace(' ','') for cc in wcsin.cunit])
				catParNames = tuple(list(catParNames) + [(cc.split('--')[0]).lower() for cc in wcsin.ctype])
				catParFormt = tuple(list(catParFormt) + ['%15.7e', '%15.7e', '%15.7e'])

			else:
				# constrain the RA axis reference value CRVAL_ to be between 0 and 360 deg
				rafound=0
				for kk in range(header['naxis']):
					if header['ctype1'][:2]=='RA':
						rafound=1
						break
				if rafound:
					if header['crval%i'%(kk+1)]<0:
						sys.stderr.write("WARNING: adding 360 deg to RA reference value.\n")
						header['crval%i'%(kk+1)]+=360
					elif header['crval%i'%(kk+1)]>360:
						sys.stderr.write("WARNING: subtracting 360 deg from RA reference value.\n")
						header['crval%i'%(kk+1)]-=360

				wcsin = wcs.WCS(header)
				xyz=objects[:,catParNames.index('x'):catParNames.index('x')+3].astype(float)
				if 'cellscal' in header and header['cellscal'] == '1/F':
					print 'WARNING: CELLSCAL keyword with value 1/F found.'
					print 'Will take into account varying pixel scale when calculating wcs coordinates.'
					x0,y0=header['crpix1']-1,header['crpix2']-1
					# Will calculate the pixscale factor of each channel as:
					# pixscale = ref_frequency / frequency
					if header['ctype3']=='VELO-HEL':
						pixscale=(1-header['crval3']/2.99792458e+8)/(1-(((xyz[:,2]+1)-header['crpix3'])*header['cdelt3']+header['crval3'])/2.99792458e+8)
					else:
						sys.stderr.write("WARNING: Cannot convert axis3 coordinates to frequency. Will ignore the effect of CELLSCAL = 1/F.\n")
						pixscale=1.
					xyz[:,0]=(xyz[:,0]-x0)*pixscale+x0
					xyz[:,1]=(xyz[:,1]-y0)*pixscale+y0
				if header['naxis']==4: objects=np.concatenate((objects,wcsin.wcs_pix2world(np.concatenate((xyz,np.zeros((objects.shape[0],1))),axis=1),0)[:,:-1]),axis=1)
				else: objects=np.concatenate((objects,wcsin.wcs_pix2world(xyz,0)),axis=1)
				catParUnits = tuple(list(catParUnits) + [str(cc).replace(' ','') for cc in wcsin.wcs.cunit])
				catParNames = tuple(list(catParNames) + [(cc.split('--')[0]).lower() for cc in wcsin.wcs.ctype])
				catParFormt = tuple(list(catParFormt) + ['%15.7e', '%15.7e', '%15.7e'])
			if header['naxis']==4:
				catParUnits = catParUnits[:-1]
				catParNames= catParNames[:-1]
			print "WCS coordinates added to the catalogue."
				
		except:
			sys.stderr.write("WARNING: WCS conversion of parameters failed.\n")
			
	return(objects, catParNames, catParFormt, catParUnits)
