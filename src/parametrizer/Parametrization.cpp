#include <iostream>
#include <cmath>
#include <limits>

#include "helperFunctions.h"
#include "DataCube.h"
#include "Parametrization.h"
#include "BusyFit.h"
#include "Measurement.h"

Parametrization::Parametrization()
{
    dataCube = 0;
    maskCube = 0;
    source   = 0;
    
    centroidX            = 0.0;
    centroidY            = 0.0;
    centroidZ            = 0.0;
    lineWidthW20         = 0.0;
    lineWidthW50         = 0.0;
    lineWidthWm50        = 0.0;
    meanFluxWm50         = 0.0;
    peakFlux             = 0.0;
    totalFlux            = 0.0;
	intSNR               = 0.0;
    busyFitSuccess       = 0;
    busyFunctionChi2     = 0.0;
    busyFunctionCentroid = 0.0;
    busyFunctionW20      = 0.0;
    busyFunctionW50      = 0.0;
    busyFunctionFpeak    = 0.0;
    busyFunctionFint     = 0.0;
    
    noiseSubCube         = 0.0;
    
    for(size_t i = 0; i < BUSYFIT_FREE_PARAM; i++)
    {
        busyFitParameters[i]    = 0.0;
        busyFitUncertainties[i] = 0.0;
    }
    
    return;
}

int Parametrization::parametrize(DataCube<float> *d, DataCube<short> *m, Source *s, bool doBF)
{
    doBusyFunction = doBF;
    
    if(loadData(d, m, s) != 0)
    {
        std::cerr << "Error (Parametrization): No data found; source parametrisation failed." << std::endl;
        return 1;
    }
    
    if(createIntegratedSpectrum() != 0)
    {
        std::cerr << "Error (Parametrization): Failed to create integrated spectrum." << std::endl;
        return 1;
    }
    
    if(measureCentroid() != 0)
    {
        std::cerr << "Warning (Parametrization): Failed to measure source centroid." << std::endl;
    }
    
    if(measureFlux() != 0)
    {
        std::cerr << "Warning (Parametrization): Source flux measurement failed." << std::endl;
    }
    
    if(measureLineWidth() != 0)
    {
        std::cerr << "Warning (Parametrization): Failed to measure source line width." << std::endl;
    }
    
    if(fitEllipse() != 0)
    {
        std::cerr << "Warning (Parametrization): Ellipse fit failed." << std::endl;
    }
    
    if(doBusyFunction == true)
    {
        if(fitBusyFunction() != 0)
        {
            std::cerr << "Warning (Parametrization): Failed to fit Busy Function." << std::endl;
        }
    }
    
    if(writeParameters() != 0)
    {
        std::cerr << "Error (Parametrization): Failed to write parameters to source." << std::endl;
        return 1;
    }
    
    return 0;
}

int Parametrization::loadData(DataCube<float> *d, DataCube<short> *m, Source *s)
{
    dataCube = 0;
    maskCube = 0;
    source = 0;
    
    data.clear();            // Clear all previously defined data.
    
    if(d == 0 or m == 0 or s == 0)
    {
        std::cerr << "Error (Parametrization): Cannot load data; invalid pointer provided." << std::endl;
        return 1;
    }
    
    if(!d->isDefined() or !m->isDefined() or !s->isDefined())
    {
        std::cerr << "Error (Parametrization): Cannot load data; source or data cube undefined." << std::endl;
        return 1;
    }
    
    if(d->getSize(0) != m->getSize(0) or d->getSize(1) != m->getSize(1) or d->getSize(2) != m->getSize(2))
    {
        std::cerr << "Error (Parametrization): Mask and data cube have different sizes." << std::endl;
        return 1;
    }
    
    double posX = s->getParameter("x");
    double posY = s->getParameter("y");
    double posZ = s->getParameter("z");
    
    if(posX < 0.0 or posY < 0.0 or posZ < 0.0 or posX >= static_cast<double>(d->getSize(0)) or posY >= static_cast<double>(d->getSize(1)) or posZ >= static_cast<double>(d->getSize(2)))
    {
        std::cerr << "Error (Parametrization): Source position outside cube range." << std::endl;
        return 1;
    }
    
    dataCube = d;
    maskCube = m;
    source   = s;
    
    // Define sub-region to operate on:
    
    if(source->parameterDefined("x_min") and source->parameterDefined("x_max"))
    {
        searchRadiusX = static_cast<long>(source->getParameter("x_max") - source->getParameter("x_min"));
    }
    else
    {
        searchRadiusX = PARAMETRIZATION_DEFAULT_SPATIAL_RADIUS;
        std::cerr << "Warning (MaskOptimization): No bounding box defined; using default search radius\n";
        std::cerr << "                            in the spatial domain instead." << std::endl;
    }
    
    if(source->parameterDefined("y_min") and source->parameterDefined("y_max"))
    {
        searchRadiusY = static_cast<long>(source->getParameter("y_max") - source->getParameter("y_min"));
    }
    else
    {
        searchRadiusY = PARAMETRIZATION_DEFAULT_SPATIAL_RADIUS;
        std::cerr << "Warning (MaskOptimization): No bounding box defined; using default search radius\n";
        std::cerr << "                            in the spatial domain instead." << std::endl;
    }
    
    if(source->parameterDefined("z_min") and source->parameterDefined("z_max"))
    {
        searchRadiusZ = static_cast<long>(0.6 * (source->getParameter("z_max") - source->getParameter("z_min")));
    }
    else
    {
        searchRadiusZ = PARAMETRIZATION_DEFAULT_SPECTRAL_RADIUS;
        std::cerr << "Warning (MaskOptimization): No bounding box defined; using default search radius\n";
        std::cerr << "                            in the spectral domain instead." << std::endl;
    }
    
    subRegionX1 = static_cast<long>(posX) - searchRadiusX;
    subRegionX2 = static_cast<long>(posX) + searchRadiusX;
    subRegionY1 = static_cast<long>(posY) - searchRadiusY;
    subRegionY2 = static_cast<long>(posY) + searchRadiusY;
    subRegionZ1 = static_cast<long>(posZ) - searchRadiusZ;
    subRegionZ2 = static_cast<long>(posZ) + searchRadiusZ;
    
    if(subRegionX1 < 0L) subRegionX1 = 0L;
    if(subRegionY1 < 0L) subRegionY1 = 0L;
    if(subRegionZ1 < 0L) subRegionZ1 = 0L;
    if(subRegionX2 >= dataCube->getSize(0)) subRegionX2 = dataCube->getSize(0) - 1L;
    if(subRegionY2 >= dataCube->getSize(1)) subRegionY2 = dataCube->getSize(1) - 1L;
    if(subRegionZ2 >= dataCube->getSize(2)) subRegionZ2 = dataCube->getSize(2) - 1L;
    
    // Extract all pixels belonging to the source and calculate local noise:
    noiseSubCube   = 0.0;
	std::vector<double> rmsMad;
    
    for(long x = subRegionX1; x <= subRegionX2; x++)
    {
        for(long y = subRegionY1; y <= subRegionY2; y++)
        {
            for(long z = subRegionZ1; z <= subRegionZ2; z++)
            {
                // Add only those pixels that are masked as being part of the source:
                float tmpFlux = dataCube->getData(x, y, z);
                
                if(static_cast<unsigned short>(maskCube->getData(x, y, z)) == source->getSourceID())
                {
                    struct DataPoint dataPoint;
                    
                    dataPoint.x = x;
                    dataPoint.y = y;
                    dataPoint.z = z;
                    dataPoint.value = tmpFlux;
                    // WARNING: A std::bad_alloc exception occurs later on when dataPoint.value is set to a constant of 1.0! No idea why...
					// NOTE:    This may not be the case any longer according to a quick test on 2 April 2015.
                    
                    data.push_back(dataPoint);
                }
                else if(maskCube->getData(x, y, z) == 0 and not std::isnan(tmpFlux))
                {
					rmsMad.push_back(static_cast<double>(tmpFlux));
                }
            }
        }
    }
    
    if(data.empty() == true)
    {
        std::cerr << "Error (Parametrization): No data found for source " << source->getSourceID() << "." << std::endl;
        return 1;
    }
    
    if(rmsMad.size() == 0)
    {
        std::cerr << "Warning (Parametrization): Noise calculation failed for source " << source->getSourceID() << "." << std::endl;
    }
    else
    {
		// Calculate the rms via the median absolute deviation (MAD):
		double m = median(rmsMad);
		for(size_t i = 0; i < rmsMad.size(); i++) rmsMad[i] = fabs(rmsMad[i] - m);
		noiseSubCube = 1.4826 * median(rmsMad);
		// NOTE: The factor of 1.4826 above is the approximate theoretical conversion factor
		//       between the MAD and the standard deviation under the assumption that the 
		//       data samples follow a normal distribution.
    }
    
    return 0;
}



// Measure centroid:

int Parametrization::measureCentroid()
{
    if(data.empty() or spectrum.empty())
    {
        std::cerr << "Error (Parametrization): No data loaded." << std::endl;
        return 1;
    }
    
    double sum = 0.0;
    centroidX  = 0.0;
    centroidY  = 0.0;
    centroidZ  = 0.0;
    
    for(size_t i = 0; i < data.size(); i++)
    {
        if(data[i].value > 0.0)        // NOTE: Only positive pixels considered here!
        {
            centroidX += data[i].value * data[i].x;
            centroidY += data[i].value * data[i].y;
            centroidZ += data[i].value * data[i].z;
            sum       += data[i].value;
        }
    }
    
    centroidX /= sum;
    centroidY /= sum;
    centroidZ /= sum;
    
    return 0;
}



// Measure peak flux, integrated flux and integrated S/N:

int Parametrization::measureFlux()
{
    if(data.empty() == true)
    {
        std::cerr << "Error (Parametrization): No data loaded." << std::endl;
        return 1;
    }
    
    totalFlux = 0.0;
    peakFlux  = -std::numeric_limits<double>::max();
    
    // Sum over all pixels (including negative ones):
    for(unsigned long i = 0L; i < data.size(); i++)
    {
        totalFlux  += static_cast<double>(data[i].value);
        if(peakFlux < static_cast<double>(data[i].value)) peakFlux = static_cast<double>(data[i].value);
    }
    
    // Calculate integrated SNR:
    intSNR = totalFlux / (noiseSubCube * sqrt(static_cast<double>(data.size())));
	// WARNING The integrated SNR would need to be divided by the beam solid angle,
	// WARNING but this will need to be done outside this module as no header
	// WARNING information known at this stage. The current value would only be
	// WARNING correct if the noise in adjacent pixels were uncorrelated.
    
    return 0;
}



// Fit ellipse to source:

int Parametrization::fitEllipse()
{
    if(data.empty() == true)
    {
        std::cerr << "Error (Parametrization): No data loaded." << std::endl;
        return 1;        
    }
    
    if(totalFlux <= 0.0)
    {
        std::cerr << "Error (Parametrization): Cannot fit ellipse, source flux <= 0." << std::endl;
        return 1;        
    }
    
    double momX  = 0.0;
    double momY  = 0.0;
    double momXY = 0.0;
    double sum   = 0.0;
    
    for(unsigned long i = 0L; i < data.size(); i++)
    {
        double fluxValue = static_cast<double>(data[i].value);
        
        if(fluxValue > 0.0)           // NOTE: Only positive pixels considered here!
        {
            momX  += (static_cast<double>(data[i].x) - source->getParameter("x")) * (static_cast<double>(data[i].x) - source->getParameter("x")) * fluxValue;
            momY  += (static_cast<double>(data[i].y) - source->getParameter("y")) * (static_cast<double>(data[i].y) - source->getParameter("y")) * fluxValue;
            momXY += (static_cast<double>(data[i].x) - source->getParameter("x")) * (static_cast<double>(data[i].y) - source->getParameter("y")) * fluxValue;
            sum += fluxValue;
        }
    }
    
    momX  /= sum;
    momY  /= sum;
    momXY /= sum;
    
    ellPA  = 0.5 * atan2(2.0 * momXY, momX - momY);
    ellMaj = sqrt(2.0 * (momX + momY + sqrt((momX - momY) * (momX - momY) + 4.0 * momXY * momXY)));
    ellMin = sqrt(2.0 * (momX + momY - sqrt((momX - momY) * (momX - momY) + 4.0 * momXY * momXY)));
    
    // WARNING: Converting PA from rad to deg:
    ellPA *= 180.0 / MATH_CONST_PI;
    
    // WARNING: Adding 90° to PA here, because astronomers like to have 0° corresponding to north.
    //          This means that PA will no longer have the mathematically correct orientation!
    ellPA += 90.0;
    
    // NOTE:    PA should now be between 0° and 180°.
    
    return 0;
}



// Create integrated spectrum:

int Parametrization::createIntegratedSpectrum()
{
    if(data.empty() == true)
    {
        std::cerr << "Error (Parametrization): No data loaded." << std::endl;
        return 1;
    }
    
    spectrum.clear();
    noiseSpectrum.clear();
    
    for(long i = 0; i <= subRegionZ2 - subRegionZ1; i++)
    {
        spectrum.push_back(0.0);
        noiseSpectrum.push_back(0.0);
    }
    
    std::vector<size_t> counter(spectrum.size(), 0);
    
    // Extract spectrum...
    for(size_t i = 0; i < data.size(); i++)
    {
        spectrum[data[i].z - subRegionZ1] += static_cast<double>(data[i].value);
        counter[data[i].z - subRegionZ1]  += 1;
    }
    
    // ...and determine noise per channel:
    // WARNING: This still needs some consideration. Is is wise to provide the rms per
    // WARNING: channel as the uncertainty? Or would one rather use the S/N? If the rms
    // WARNING: is zero (because there are no data) the BF fitting will just produce NaNs.
    // WARNING: Even worse: the noise will not scale with sqrt(N), because pixels are
    // WARNING: spatially correlated!!!
    for(size_t i = 0; i < noiseSpectrum.size(); i++)
    {
        if(counter[i] > 0) noiseSpectrum[i] = sqrt(static_cast<double>(counter[i])) * noiseSubCube;
        else noiseSpectrum[i] = std::numeric_limits<double>::infinity();
    }
    
    return 0;
}



// Measure line width:

int Parametrization::measureLineWidth()
{
    if(data.empty() == true or spectrum.empty() == true)
    {
        std::cerr << "Error (Parametrization): No data loaded." << std::endl;
        return 1;
    }
    
    // Determine maximum:
    double specMax = 0.0;    // WARNING: This assumes that sources are always positive!
    
    for(size_t i = 0; i < spectrum.size(); i++)
    {
        if(spectrum[i] > specMax) specMax = spectrum[i];
    }
    
    //Determine w₅₀:
    size_t i = 0;
    
    while(i < spectrum.size() and spectrum[i] < specMax / 2.0) i++;
    
    if(i >= spectrum.size())
    {
        std::cerr << "Error (Parametrization): Calculation of W50 failed (1)." << std::endl;
        lineWidthW50 = 0.0;
        return 1;
    }
    
    lineWidthW50 = static_cast<double>(i);
    if(i > 0) lineWidthW50 -= (spectrum[i] - specMax / 2.0) / (spectrum[i] - spectrum[i - 1]);     // Interpolate if not at edge.
    
    i = spectrum.size() - 1;
    
    while(i >= 0 and spectrum[i] < specMax / 2.0) i--;
    
    if(i < 0)
    {
        std::cerr << "Error (Parametrization): Calculation of W50 failed (2)." << std::endl;
        lineWidthW50 = 0.0;
        return 1;
    }
    
    lineWidthW50 = static_cast<double>(i) - lineWidthW50;
    if(i < spectrum.size() - 1) lineWidthW50 += (spectrum[i] - specMax / 2.0) / (spectrum[i] - spectrum[i + 1]);  // Interpolate if not at edge.
    
    if(lineWidthW50 <= 0.0)
    {
        std::cerr << "Error (Parametrization): Calculation of W50 failed (3)." << std::endl;
        lineWidthW50 = 0.0;
        return 1;
    }
    
    //Determine w₂₀:
    i = 0;
    
    while(i < spectrum.size() and spectrum[i] < specMax / 5.0) i++;
    
    if(i >= spectrum.size())
    {
        std::cerr << "Error (Parametrization): Calculation of W20 failed (1)." << std::endl;
        lineWidthW20 = 0.0;
        return 1;
    }
    
    lineWidthW20 = static_cast<double>(i);
    if(i > 0) lineWidthW20 -= (spectrum[i] - specMax / 5.0) / (spectrum[i] - spectrum[i - 1]);     // Interpolate if not at edge.
    
    i = spectrum.size() - 1;
    
    while(i >= 0 and spectrum[i] < specMax / 5.0) i--;
    
    if(i < 0)
    {
        std::cerr << "Error (Parametrization): Calculation of W20 failed (2)." << std::endl;
        lineWidthW20 = 0.0;
        return 1;
    }
    
    lineWidthW20 = static_cast<double>(i) - lineWidthW20;
    if(i < spectrum.size() - 1) lineWidthW20 += (spectrum[i] - specMax / 5.0) / (spectrum[i] - spectrum[i + 1]);  // Interpolate if not at edge.
    
    if(lineWidthW20 <= 0.0)
    {
        std::cerr << "Error (Parametrization): Calculation of W20 failed (3)." << std::endl;
        lineWidthW20 = 0.0;
        return 1;
    }
    
    //Determine Wₘ₅₀:
    double sum = 0.0;
    double bound90l = 0.0;
    double bound90r = 0.0;
    lineWidthWm50 = 0.0;
    meanFluxWm50 = 0.0;
    i = 0;
    
    while(sum < 0.05 * totalFlux and i < spectrum.size())
    {
        sum += spectrum[i];
        i++;
    }
    
    i--;
    
    if(i >= spectrum.size())
    {
        std::cerr << "Error (Parametrization): Calculation of Wm50 failed." << std::endl;
        return 1;
    }
    
    bound90l = static_cast<double>(i);
    if(i > 0) bound90l -= (spectrum[i] - 0.05 * totalFlux) / (spectrum[i] - spectrum[i - 1]);      // Interpolate if not at edge.
    
    i = spectrum.size() - 1;
    sum = 0.0;
    
    while(sum < 0.05 * totalFlux and i >= 0)
    {
        sum += spectrum[i];
        i--;
    }
    
    i++;
    
    if(i < 0)
    {
        std::cerr << "Error (Parametrization): Calculation of Wm50 failed." << std::endl;
        return 1;
    }
    
    bound90r = static_cast<double>(i);
    if(i < spectrum.size() - 1) bound90r += (spectrum[i] - 0.05 * totalFlux) / (spectrum[i] - spectrum[i + 1]);   // Interpolate if not at edge.
    
    meanFluxWm50 = 0.9 * totalFlux / (bound90r - bound90l);
    
    if(meanFluxWm50 <= 0)
    {
        std::cerr << "Error (Parametrization): Calculation of Wm50 failed." << std::endl;
        meanFluxWm50 = 0.0;
        return 1;
    }
    
    i = 0;
    
    while(spectrum[i] < 0.5 * meanFluxWm50 and i < spectrum.size()) i++;
    
    if(i >= spectrum.size())
    {
        std::cerr << "Error (Parametrization): Calculation of Wm50 failed." << std::endl;
        meanFluxWm50 = 0.0;
        return 1;
    }
    
    lineWidthWm50 = static_cast<double>(i);
    if(i > 0) lineWidthWm50 -= (spectrum[i] - 0.5 * meanFluxWm50) / (spectrum[i] - spectrum[i - 1]);      // Interpolate if not at edge.
    
    i = spectrum.size() - 1;
    
    while(spectrum[i] < 0.5 * meanFluxWm50 and i >= 0) i--;
    
    if(i < 0)
    {
        std::cerr << "Error (Parametrization): Calculation of Wm50 failed." << std::endl;
        lineWidthWm50 = 0.0;
        meanFluxWm50 = 0.0;
        return 1;
    }
    
    lineWidthWm50 = static_cast<double>(i) - lineWidthWm50;
    if(i < spectrum.size() - 1) lineWidthWm50 += (spectrum[i] - 0.5 * meanFluxWm50) / (spectrum[i] - spectrum[i + 1]);   // Interpolate if not at edge.
    
    if(lineWidthWm50 <= 0)
    {
        std::cerr << "Error (Parametrization): Calculation of Wm50 failed." << std::endl;
        lineWidthWm50 = 0.0;
        meanFluxWm50 = 0.0;
        return 1;
    }
    
    return 0;
}



// Fit Busy Function:

int Parametrization::fitBusyFunction()
{
    if(data.empty() or spectrum.empty())
    {
        std::cerr << "Error (Parametrization): No data loaded." << std::endl;
        return 1;
    }
    
    BusyFit busyFit;
    busyFit.setup(spectrum.size(), &spectrum[0], &noiseSpectrum[0], 2, true, false);
    
    busyFitSuccess = busyFit.fit();
    
    busyFit.getResult(&busyFitParameters[0], &busyFitUncertainties[0], busyFunctionChi2);
    busyFit.getParameters(busyFunctionCentroid, busyFunctionW50, busyFunctionW20, busyFunctionFpeak, busyFunctionFint);
    
    // Correct spectral parameters for the shift caused by using a sub-cube:
    busyFitParameters[4] += subRegionZ1;
    busyFitParameters[5] += subRegionZ1;
    busyFunctionCentroid += subRegionZ1;
    
    return 0;
}



// Assign results to source:

int Parametrization::writeParameters()
{
    source->setParameter("id",      source->getSourceID());
    source->setParameter("x",       centroidX);
    source->setParameter("y",       centroidY);
    source->setParameter("z",       centroidZ);
    source->setParameter("w50",     lineWidthW50);
    source->setParameter("w20",     lineWidthW20);
    source->setParameter("wm50",    lineWidthWm50);
    source->setParameter("f_wm50",  meanFluxWm50);
    source->setParameter("f_peak",  peakFlux);
	source->setParameter("f_int",   totalFlux);
	source->setParameter("snr_int", intSNR);
    
    source->setParameter("ell_maj", ellMaj);
    source->setParameter("ell_min", ellMin);
    source->setParameter("ell_pa",  ellPA);
    
    source->setParameter("rms",     noiseSubCube);
    
    if(doBusyFunction == true)
    {
        source->setParameter("bf_flag",   busyFitSuccess);
        source->setParameter("bf_chi2",   busyFunctionChi2);
        source->setParameter("bf_a",      busyFitParameters[0]);
        source->setParameter("bf_b1",     busyFitParameters[1]);
        source->setParameter("bf_b2",     busyFitParameters[2]);
        source->setParameter("bf_c",      busyFitParameters[3]);
        source->setParameter("bf_xe",     busyFitParameters[4]);
        source->setParameter("bf_xp",     busyFitParameters[5]);
        source->setParameter("bf_w",      busyFitParameters[6]);
        source->setParameter("bf_z",      busyFunctionCentroid);
        source->setParameter("bf_w20",    busyFunctionW20);
        source->setParameter("bf_w50",    busyFunctionW50);
        source->setParameter("bf_f_peak", busyFunctionFpeak);
        source->setParameter("bf_f_int",  busyFunctionFint);
    }
    
    return 0;
}
