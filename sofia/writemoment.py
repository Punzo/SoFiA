#! /usr/bin/env python
import astropy.io.fits as pyfits
import os
import numpy as np
from .version import *

def removeOptions(dictionary):
	modDictionary = dictionary
	for key in modDictionary['steps']:
		if modDictionary['steps'][key] == 'False':
			modDictionary.pop(key.split('do')[1].lower(), None)
	return modDictionary


def recursion(dictionary, optionsList, optionsDepth, counter=0):
	if type(dictionary) == type({}):
		for k in dictionary:
			optionsList.append(str(k))
			optionsDepth.append(counter)
			recursion(dictionary[k], optionsList, optionsDepth, counter=counter+1)
	else:
		optionsList[len(optionsList) - 1] += '=' + str(dictionary)
		counter = 0

def writeMoment0(datacube,maskcube,filename,debug,header,compress):
	print ('Writing moment-0') # in units of header['bunit']*km/s
	m0 = datacube * maskcube.astype(bool)
	m0 = np.array((np.ma.array(m0, mask=(~np.isfinite(m0)), fill_value=0).sum(axis=0)).filled())
	op = 0
	
	if 'vopt' in header['ctype3'].lower() or 'vrad' in header['ctype3'].lower() or 'velo' in header['ctype3'].lower() or 'felo' in header['ctype3'].lower():
		if not 'cunit3' in header:
			dkms = abs(header['cdelt3']) / 1e+3 # assuming m/s
		elif header['cunit3'].lower() == 'km/s':
			dkms = abs(header['cdelt3'])
	elif 'freq' in header['ctype3'].lower():
		if not 'cunit3' in header or header['cunit3'].lower()=='hz':
			dkms = abs(header['cdelt3']) / 1.42040575177e+9 * 2.99792458e+5 # assuming Hz
		elif header['cunit3'].lower() == 'khz':
			dkms = abs(header['cdelt3']) / 1.42040575177e+6 * 2.99792458e+5
	else: dkms = 1.0 # no scaling, avoids crashing
	
	hdu = pyfits.PrimaryHDU(data=m0*dkms, header=header)
	hdu.header['bunit'] += '.km/s'
	hdu.header['datamin'] = (m0 * dkms).min()
	hdu.header['datamax'] = (m0 * dkms).max()
	hdu.header['ORIGIN'] = getVersion(full=True)
	del(hdu.header['crpix3'])
	del(hdu.header['crval3'])
	del(hdu.header['cdelt3'])
	del(hdu.header['ctype3'])
	
	if debug:
		hdu.writeto('%s_mom0.debug.fits' % filename, output_verify='warn', clobber=True)
	else: 
		name = '%s_mom0.fits' % filename
		if compress: name += '.gz'
		hdu.writeto(name, output_verify='warn', clobber=True)
	return m0

def writeMoment1(datacube, maskcube, filename, debug, header, m0, compress):
	print ('Writing moment-1')
	# create array of axis3 coordinates
	m1 = (np.arange(datacube.shape[0]).reshape((datacube.shape[0], 1, 1)) * np.ones(datacube.shape) - header['crpix3'] + 1) * header['cdelt3'] + header['crval3'] # in axis3 units
	
	# convert it to km/s (using radio velocity definition to go from Hz to km/s)
	if 'vopt' in header['ctype3'].lower() or 'vrad' in header['ctype3'].lower() or 'velo' in header['ctype3'].lower() or 'felo' in header['ctype3'].lower():
		if not 'cunit3' in header: m1 /= 1e+3 # assuming m/s
		elif header['cunit3'].lower() == 'km/s': pass
	elif 'freq' in header['ctype3'].lower():
		if not 'cunit3' in header or header['cunit3'].lower() == 'hz': m1 *= 2.99792458e+5 / 1.42040575177e+9 # assuming Hz
		elif header['cunit3'].lower() == 'khz': m1 *= 2.99792458e+5 / 1.42040575177e+6
	
	# calculate moment 1
	m0[m0 == 0] = np.nan
	m1=np.divide(np.array(np.nan_to_num(m1 * datacube * maskcube.astype('bool')).sum(axis=0)),m0)
	hdu = pyfits.PrimaryHDU(data=m1, header=header)
	hdu.header['bunit'] = 'km/s'
	hdu.header['datamin'] = np.nanmin(m1)
	hdu.header['datamax'] = np.nanmax(m1)
	hdu.header['ORIGIN'] = getVersion(full=True)
	del(hdu.header['crpix3'])
	del(hdu.header['crval3'])
	del(hdu.header['cdelt3'])
	del(hdu.header['ctype3'])
	
	if debug:
		hdu.writeto('%s_mom1.debug.fits' % filename, output_verify='warn', clobber=True)
	else:
		name = '%s_mom1.fits' % filename
		if compress: name += '.gz'
		hdu.writeto(name, output_verify='warn', clobber=True)
