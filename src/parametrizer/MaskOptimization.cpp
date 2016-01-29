#include <iostream>
#include <cmath>

#include "helperFunctions.h"
#include "MaskOptimization.h"

MaskOptimization::MaskOptimization(long dx, long dy, long dz)
{
	dataCube = 0;
	maskCube = 0;
	source   = 0;
	
	ellA  = 0.0;
	ellB  = 0.0;
	theta = 0.0;
	
	// dx, dy, and dz all default to 0; sensible defaults will be chosen later in loadData().
	searchRadiusX = dx;
	searchRadiusY = dy;
	searchRadiusZ = dz;
	
	return;
}



// Function to optimise mask:

int MaskOptimization::optimize(DataCube<float> *d, DataCube<short> *m, Source *s)
{
	if(loadData(d, m, s) != 0)
	{
		std::cerr << "Error (MaskOptimization): Mask optimisation failed.\n";
		return 1;
	}
	
	if(createMomentMap() != 0)
	{
		std::cerr << "Error (MaskOptimization): Mask optimisation failed.\n";
		return 1;
	}
	
	if(fitEllipse() != 0)
	{
		std::cerr << "Error (MaskOptimization): Mask optimisation failed.\n";
		return 1;
	}
	
	if(growEllipse() != 0)
	{
		std::cerr << "Error (MaskOptimization): Mask optimisation failed.\n";
		return 1;
	}
	
	return 0;
}



// Function to load and check data:

int MaskOptimization::loadData(DataCube<float> *d, DataCube<short> *m, Source *s)
{
	if(d == 0 or m == 0 or s == 0)
	{
		std::cerr << "Error (MaskOptimization): Cannot load data; invalid pointer provided.\n";
		return 1;
	}
	
	if(!d->isDefined() or !m->isDefined() or !s->isDefined())
	{
		std::cerr << "Error (MaskOptimization): Cannot load data; source or data cube undefined.\n";
		return 1;
	}
	
	if(d->getSize(0) != m->getSize(0) or d->getSize(1) != m->getSize(1) or d->getSize(2) != m->getSize(2))
	{
		std::cerr << "Error (MaskOptimization): Mask and data cube have different sizes.\n";
		return 1;
	}
	
	double x = s->getParameter("x");
	double y = s->getParameter("y");
	double z = s->getParameter("z");
	
	if(x < 0.0 or y < 0.0 or z < 0.0 or x >= static_cast<double>(d->getSize(0)) or y >= static_cast<double>(d->getSize(1)) or z >= static_cast<double>(d->getSize(2)))
	{
		std::cerr << "Error (MaskOptimization): Source position outside cube boundaries.\n";
		return 1;
	}
	
	dataCube = d;
	maskCube = m;
	source   = s;
	
	// Define sub-region to operate on:
	
	if(searchRadiusX <= 0L)
	{
		if(source->parameterDefined("x_min") and source->parameterDefined("x_max"))
		{
			searchRadiusX = static_cast<long>(source->getParameter("x_max") - source->getParameter("x_min"));
		}
		else
		{
			searchRadiusX = MASKOPTIMIZATION_DEFAULT_SPATIAL_RADIUS;
			std::cerr << "Warning (MaskOptimization): No bounding box defined; using default search radius\n";
			std::cerr << "                            in the spatial domain instead.\n";
		}
	}
	
	if(searchRadiusY <= 0L)
	{
		if(source->parameterDefined("y_min") and source->parameterDefined("y_max"))
		{
			searchRadiusY = static_cast<long>(source->getParameter("y_max") - source->getParameter("y_min"));
		}
		else
		{
			searchRadiusY = MASKOPTIMIZATION_DEFAULT_SPATIAL_RADIUS;
			std::cerr << "Warning (MaskOptimization): No bounding box defined; using default search radius\n";
			std::cerr << "                            in the spatial domain instead.\n";
		}
	}
	
	if(searchRadiusZ <= 0L)
	{
		//double dz;
		
		if(source->parameterDefined("z_min") and source->parameterDefined("z_max"))
		{
			searchRadiusZ = static_cast<long>(0.6 * (source->getParameter("z_max") - source->getParameter("z_min")));
		}
		/*else if(source->parameterDefined("W50") and !dataCube->getHeader("cdelt3", dz))
		 *        {
		 *            // WARNING This assumes that the units of w50 of the source are the same as
		 *            // WARNING those of cdelt3 in the data cube!!! However, this may not be the case!
		 *            // WARNING Either we need to ensure that they are always the same, or we have 
		 *            // WARNING to compare units at this point!
		 *            searchRadiusZ = static_cast<long>(source->getParameter("W50") / mathAbs(dz));
	}*/
		else
		{
			searchRadiusZ = MASKOPTIMIZATION_DEFAULT_SPECTRAL_RADIUS;
			std::cerr << "Warning (MaskOptimization): No bounding box defined; using default search radius\n";
			std::cerr << "                            in the spectral domain instead.\n";
		}
	}
	
	subRegionX1 = static_cast<long>(x) - searchRadiusX;
	subRegionX2 = static_cast<long>(x) + searchRadiusX;
	subRegionY1 = static_cast<long>(y) - searchRadiusY;
	subRegionY2 = static_cast<long>(y) + searchRadiusY;
	subRegionZ1 = static_cast<long>(z) - searchRadiusZ;
	subRegionZ2 = static_cast<long>(z) + searchRadiusZ;
	
	if(subRegionX1 < 0L) subRegionX1 = 0L;
	if(subRegionY1 < 0L) subRegionY1 = 0L;
	if(subRegionZ1 < 0L) subRegionZ1 = 0L;
	if(subRegionX2 >= dataCube->getSize(0)) subRegionX2 = dataCube->getSize(0) - 1L;
	if(subRegionY2 >= dataCube->getSize(1)) subRegionY2 = dataCube->getSize(1) - 1L;
	if(subRegionZ2 >= dataCube->getSize(2)) subRegionZ2 = dataCube->getSize(2) - 1L;
	
	return 0;
}



// Function to create moment-zero map:
// WARNING: No cut-off is allowed here, because moment map used later in ellipse-growing!

int MaskOptimization::createMomentMap()
{
	long dx = subRegionX2 - subRegionX1 + 1L;
	long dy = subRegionY2 - subRegionY1 + 1L;
	long dz = subRegionZ2 - subRegionZ1 + 1L;
	
	if(!dataCube->isDefined() or !maskCube->isDefined() or !source->isDefined())
	{
		std::cerr << "Error (MaskOptimization): Cannot create moment map; no data provided.\n";
		return 1;
	}
	
	if(momentMap.createNewCube(dx, dy, 1L))
	{
		std::cerr << "Error (MaskOptimization): Failed to create moment map.\n";
		return 1;
	}
	
	if(momentMap.floodFill(0.0))
	{
		std::cerr << "Error (MaskOptimization): Initialisation of moment map failed.\n";
		return 1;
	}
	
	for(long x = 0L; x < dx; x++)
	{
		for(long y = 0L; y < dy; y++)
		{
			for(long z = 0L; z < dz; z++)
			{
				// Add only those pixels that are masked as being part of the source:
				if(maskCube->getData(subRegionX1 + x, subRegionY1 + y, subRegionZ1 + z) == static_cast<signed long>(source->getSourceID()))
				{
					momentMap.addData(dataCube->getData(subRegionX1 + x, subRegionY1 + y, subRegionZ1 + z), x, y, 0L);
				}
			}
		}
	}
	
	return 0;
}



// Function to fit ellipse to moment map:

int MaskOptimization::fitEllipse()
{
	if(!momentMap.isDefined())
	{
		std::cerr << "Error (MaskOptimization): Cannot fit ellipse; moment map undefined.\n";
		return 1;
	}
	
	if(!source->isDefined())
	{
		std::cerr << "Error (MaskOptimization): Cannot fit ellipse; no source specified.\n";
		return 1;
	}
	
	double momX  = 0.0;
	double momY  = 0.0;
	double momXY = 0.0;
	double sum   = 0.0;
	
	// Calculate second spatial moments:
	
	for(long x = subRegionX1; x <= subRegionX2; x++)
	{
		for(long y = subRegionY1; y < subRegionY2; y++)
		{
			double value = static_cast<double>(momentMap.getData(x - subRegionX1, y - subRegionY1, 0L));
			
			if(value > 0.0)
			{
				momX  += (static_cast<double>(x) - source->getParameter("x")) * (static_cast<double>(x) - source->getParameter("x")) * value;
				momY  += (static_cast<double>(y) - source->getParameter("y")) * (static_cast<double>(y) - source->getParameter("y")) * value;
				momXY += (static_cast<double>(x) - source->getParameter("x")) * (static_cast<double>(y) - source->getParameter("y")) * value;
				sum   += value;
			}
		}
	}
	
	if(sum <= 0.0)
	{
		std::cerr << "Error (MaskOptimization): Cannot fit ellipse; no positive flux found.\n";
		return 1;
	}
	
	momX  /= sum;
	momY  /= sum;
	momXY /= sum;
	
	theta = 0.5 * atan2(2.0 * momXY, momX - momY);
	ellA = sqrt(2.0 * (momX + momY + sqrt((momX - momY) * (momX - momY) + 4.0 * momXY * momXY)));
	ellB = sqrt(2.0 * (momX + momY - sqrt((momX - momY) * (momX - momY) + 4.0 * momXY * momXY)));
	
	//source->setParameter("ELL_MAJ", ellA);
	//source->setParameter("ELL_MIN", ellB);
	//source->setParameter("ELL_PA",  180.0 * theta / MATH_CONST_PI);
	// This is no longer needed here, because ellipse fit will be repeated in parametrisation module.
	
	// Set initial values for semi-major and semi-minor axes (as defined in header file):
	ellB = MASKOPTIMIZATION_INITIAL_ELLIPSE_SIZE * ellB / ellA;
	ellA = MASKOPTIMIZATION_INITIAL_ELLIPSE_SIZE;
	
	return 0;
}



// Function to grow ellipse to maximise enclosed flux:

int MaskOptimization::growEllipse()
{
	if(!momentMap.isDefined())
	{
		std::cerr << "Error (MaskOptimization): Cannot grow ellipse; moment map undefined.\n";
		return 1;
	}
	
	if(!source->isDefined())
	{
		std::cerr << "Error (MaskOptimization): Cannot grow ellipse; no source specified.\n";
		return 1;
	}
	
	if(ellA <= 0.0 or ellB <= 0.0)
	{
		std::cerr << "Error (MaskOptimization): Cannot grow ellipse; ellipse parameters undefined.\n";
		return 1;
	}
	
	int   iteration  = 0;
	float sum        = 0.0F;
	float sumMax     = 0.0F;           // WARNING: This initialisation will discard sources with negative flux!
	
	double posX = source->getParameter("x");
	double posY = source->getParameter("y");
	
	while(sum >= sumMax and iteration <= MASKOPTIMIZATION_ITERATIONS)
	{
		iteration++;
		
		ellB += MASKOPTIMIZATION_ELLIPSE_GROWTH * ellB / ellA;       // Increase ellipse size
		ellA += MASKOPTIMIZATION_ELLIPSE_GROWTH;
		
		sum = 0.0F;
		
		for(long x = subRegionX1; x <= subRegionX2; x++)
		{
			for(long y = subRegionY1; y <= subRegionY2; y++)
			{
				// Position angle of pixel (x,y) w.r.t. source:
				double phi = atan2(static_cast<double>(y) - posY, static_cast<double>(x) - posX) - theta;
				
				// SQUARE of radius of pixel (x,y) w.r.t. source:
				double radiusPix2 = (static_cast<double>(x) - posX) * (static_cast<double>(x) - posX) + (static_cast<double>(y) - posY) * (static_cast<double>(y) - posY);
				
				// Radius of ellipse at that position angle:
				double radiusEll = ellA * ellB / sqrt(ellA * ellA * sin(phi) * sin(phi) + ellB * ellB * cos(phi) * cos(phi));
				
				if(radiusPix2 <= radiusEll * radiusEll)
				{
					// Iterate through all spectral channels:
					// This would be unnecessary when simply using the moment map instead of the cube,
					// but in this case we would not be able to avoid other sources in the mask!
					for(long z = subRegionZ1; z <= subRegionZ2; z++)
					{
						// Check mask value to ensure that pixels belonging to a different source are excluded:
						unsigned long maskValue = static_cast<unsigned long>(maskCube->getData(x, y, z));
						
						if(maskValue == 0UL or maskValue == source->getSourceID())
						{
							sum += dataCube->getData(x, y, z);
						}
					}
				}
			}
		}
		
		if(sum > sumMax) sumMax = sum;
	}
	
	ellB -= MASKOPTIMIZATION_ELLIPSE_GROWTH * ellB / ellA; // Decrease ellipse size again to revert to value in second-to-last iteration.
	ellA -= MASKOPTIMIZATION_ELLIPSE_GROWTH;               // (This is because we are past the maximum flux already.)
	
	if(iteration > MASKOPTIMIZATION_ITERATIONS or iteration <= 1)
	{
		std::cerr << "Warning (MaskOptimization): Failed to converge or flux negative\n";
		std::cerr << "                            for source " << source->getSourceID() << ".\n";
	}
	else
	{
		// Modify mask cube:
		unsigned long pixelCount = 0;
		
		for(long x = subRegionX1; x <= subRegionX2; x++)
		{
			for(long y = subRegionY1; y <= subRegionY2; y++)
			{
				// Position angle of pixel (x,y) w.r.t. source:
				double phi = atan2(static_cast<double>(y) - posY, static_cast<double>(x) - posX) - theta;
				
				// SQUARE of radius of pixel (x,y) w.r.t. source:
				double radiusPix2 = (static_cast<double>(x) - posX) * (static_cast<double>(x) - posX) + (static_cast<double>(y) - posY) * (static_cast<double>(y) - posY);
				
				// Radius of ellipse at that position angle:
				double radiusEll = ellA * ellB / sqrt(ellA * ellA * sin(phi) * sin(phi) + ellB * ellB * cos(phi) * cos(phi));
				
				if(radiusPix2 <= radiusEll * radiusEll)
				{
					// Iterate through all spectral channels:
					for(long z = subRegionZ1; z <= subRegionZ2; z++)
					{
						// Check mask value to ensure that pixels belonging to a different source are excluded:
						unsigned long maskValue = static_cast<unsigned long>(maskCube->getData(x, y, z));
						
						if(maskValue == 0UL or maskValue == source->getSourceID())
						{
							// Mask pixel:
							maskCube->setData(source->getSourceID(), x, y, z);
							++pixelCount;
							
							// Update bounding box:
							if(x < source->getParameter("x_min")) source->setParameter("x_min", x);
							if(x > source->getParameter("x_max")) source->setParameter("x_max", x);
							if(y < source->getParameter("y_min")) source->setParameter("y_min", y);
							if(y > source->getParameter("y_max")) source->setParameter("y_max", y);
							if(z < source->getParameter("z_min")) source->setParameter("z_min", z);
							if(z > source->getParameter("z_max")) source->setParameter("z_max", z);
						}
					}
				}
			}
		}
		
		source->setParameter("n_pix", pixelCount);
	}
	
	return 0;
}
