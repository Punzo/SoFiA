import numpy as np
from scipy import ndimage as nd

#from .pyudwt import Denoise2D1DHardMRS
from sofia import wavelet as wv

b3spline = np.array([1.0, 4.0, 6.0, 4.0, 1.0]) / 16.0

# Try to use MUCH faster median implementation
# from bottleneck, else fallback to numpy.median
try:
	from bottleneck import median as the_median
except ImportError:
	the_median = np.median


class Denoise2D1DAdaptiveMRS(wv.Denoise2D1DHardMRS):
	"""
	De-noise a three-dimensional data cube using the 2D1D wavelet
	transform with a multi-resolution support as discussed in 
	Starck et al. 2011 and Floeer & Winkel 2012.
	
	The transform assumes that the first axis is the spectral axis.
	"""
	
	def __init__(self, sigma, *args, **kwargs):
	
		self.sigma = sigma
	
		self.xy_mother_function = b3spline
		self.z_mother_function = b3spline
		
		super(Denoise2D1DAdaptiveMRS, self).__init__(*args, **kwargs)
		
		self.thresholds = np.zeros((self.xy_scales + 1, self.z_scales + 1))
	
	
	def handle_coefficients(self, work_array, xy_scale, z_scale):
		
		subband_rms = the_median(np.abs(self.work[work_array])) / 0.6745
		subband_threshold = subband_rms * self.sigma
		
		self.thresholds[xy_scale, z_scale] = subband_threshold
		
		super(Denoise2D1DAdaptiveMRS, self).handle_coefficients(work_array, xy_scale, z_scale)


def denoise_2d1d(data, threshold=5.0, scaleXY=-1, scaleZ=-1, positivity=True, iterations=3, valid=None, **kwargs):
	"""
	
	Inputs
	------
	
	data : 3d ndarray
	    The data to denoise. The data already has to have all weights applied to
	    it.
	
	threshold : float, optional
	    The reconstruction threshold. Gets multiplied by the noise in each
	    wavelet sub-band.
	
	scaleXY : int, optional
	    The number of spatial scales to use for decomposition.
	
	scaleZ : int, optional
	    The number of spectral scales to use for decomposition.
	
	positivity : bool, optional
	    Enforce positivity on the reconstruction
	
	iterations : int, optional
	    The number of iterations for the reconstruction process.
	
	valid : 3d ndarray of bool, optional
	    Mask indicating the valid values in the data. True means good data.
	    False values are set to 0. prior to reconstruction.
	    Gets deduced from the data if not provided.
	
	Other arguments are passed to the denoising class. Possible arguments are:
	    xy_approx : bool
	        Whether to consider the wavelet sub-bands representing the spatial
	        approximation during reconstruction
	
	    z_approx : bool
	        Whether to consider the wavelet sub-bands representing the spectral
	        approximation during reconstruction
	
	    total_power : bool
	        Whether to add the smooth approximation to the reconstruction
	
	
	Returns
	-------
	
	reconstruction : 3d ndarray
	    The reconstructed data
	"""
	
	data = np.array(data, dtype=np.single)
	
	if valid is None:
		valid = np.isfinite(data)
	
	data[~valid] = 0.
	
	denoiser = Denoise2D1DAdaptiveMRS(
		sigma=threshold,
		data=data,
		xy_scales=scaleXY,
		z_scales=scaleZ,
		**kwargs)
	
	denoiser.positivity = positivity
	
	for iteration in xrange(iterations):
		denoiser.decompose()
	
	reconstruction = denoiser.reconstruction
	reconstruction[~valid] = 0.
	
	return reconstruction
