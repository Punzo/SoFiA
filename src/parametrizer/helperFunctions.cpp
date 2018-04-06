// Compile: g++ -c helperFunctions.cpp

#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "helperFunctions.h"



// Convert std::string to upper and lower case:

int stringToUpper(std::string &strToConvert)
{
	std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);
	
	return 0;
}

int stringToLower(std::string &strToConvert)
{
	std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::tolower);
	
	return 0;
}



// Trim std::string by removing whitespace characters:
// mode = 0  ->  trim both ends
//        1  ->  trim leading whitespaces only
//        2  ->  trim trailing whitespaces only

void stringTrim(std::string &strToConvert, int mode)
{
	if(strToConvert.empty()) return;
	
	std::string ws(" \t\n\r\0\v");               // Same whitespace characters as used in PHP's trim()
	size_t foundFirst;
	size_t foundLast;
	
	if(mode < 0 or mode > 2) mode = 0;
	
	if(mode == 0)
	{
		// Trim both ends (default):
		foundFirst = strToConvert.find_first_not_of(ws);
		foundLast  = strToConvert.find_last_not_of(ws);
		
		if(foundFirst != std::string::npos and foundLast != std::string::npos)
		{
			strToConvert.assign(strToConvert.substr(foundFirst, foundLast - foundFirst + 1));
		}
		else
		{
			strToConvert.clear();                    // String contains only whitespace
		}
	}
	else if(mode == 1)
	{
		// Trim leading whitespaces:
		foundFirst = strToConvert.find_first_not_of(ws);
		
		if(foundFirst != std::string::npos)
		{
			strToConvert.assign(strToConvert.substr(foundFirst));
		}
		else
		{
			strToConvert.clear();                    // String contains only whitespace
		}
	}
	else
	{
		// Trim trailing whitespaces:
		foundLast = strToConvert.find_last_not_of(ws);
		
		if(foundLast != std::string::npos)
		{
			strToConvert.assign(strToConvert.substr(0, foundLast + 1));
		}
		else
		{
			strToConvert.clear();                    // String contains only whitespace
		}
	}
	
	return;
}



// Tokenize std::string:

int stringTok(std::string &strToConvert, std::vector<std::string> &tokens, const std::string &delimiters)
{
	tokens.clear();
	
	// Ignore initial delimiters:
	std::string::size_type firstPosition = strToConvert.find_first_not_of(delimiters, 0);
	
	if(firstPosition == std::string::npos) return 1;
	
	// Find first "true" delimiter:
	std::string::size_type firstDelim = strToConvert.find_first_of(delimiters, firstPosition);
	
	while(firstDelim != std::string::npos or firstPosition != std::string::npos)
	{
		tokens.push_back(strToConvert.substr(firstPosition, firstDelim - firstPosition));
		
		firstPosition = strToConvert.find_first_not_of(delimiters, firstDelim);
		firstDelim    = strToConvert.find_first_of(delimiters, firstPosition);
	}
	
	return 0;
}



// Replace all occurrences of search string with replacement string:

int stringReplace(std::string &strToConvert, const std::string &seachStr, const std::string &replStr)
{
	if(strToConvert.empty() or seachStr.empty()) return 1;
	
	size_t firstPos = 0;
	size_t pos = strToConvert.find(seachStr, firstPos);
	
	if(pos == std::string::npos) return 1;
	
	while(pos != std::string::npos)
	{
		strToConvert = strToConvert.substr(0, pos) + replStr + strToConvert.substr(pos + seachStr.size());
		firstPos     = pos + replStr.size();
		if(firstPos < strToConvert.size()) pos = strToConvert.find(seachStr, firstPos);
		else break;
	}
	
	return 0;
}



// Convert decimal degrees to dd:mm:ss.ss string:

std::string degToDms(double number)
{
	double number2 = fabs(number);
	int    deg;
	int    min;
	double sec;
	
	deg = static_cast<int>(number2);
	min = static_cast<int>(60.0 * (number2 - static_cast<double>(deg)));
	sec = 60.0 * (60.0 * (number2 - static_cast<double>(deg)) - static_cast<double>(min));
	
	std::ostringstream convert;
	
	if(number < 0.0) convert << "-";
	
	convert << std::fixed << std::setw(2) << std::setfill('0') << deg << ":" << std::setw(2) << std::setfill('0') << min << ":" << std::setw(5) << std::setprecision(2) << sec;
	
	return convert.str();
}



// Convert decimal degrees to hh:mm:ss.ss string:

std::string degToHms(double number)
{
	number /= 15.0;
	
	return degToDms(number);
}



// Return sign of value:

int mathSgn(int value)
{
	if(value < 0) return -1;
	else          return  1;
}



// Return absolute value of argument:

double mathAbs(double value)
{
	if(value < 0.0) return -1.0 * value;
	else return value;
}



// Round value:

long mathRound(float value)
{
	return static_cast<float>(value + 0.5);
}

long mathRound(double value)
{
	return static_cast<long>(value + 0.5);
}



// Calculate median of vector of double values:

/*double median(std::vector<double> &values)
{
	size_t sampleSize = values.size();
	
	switch(sampleSize)
	{
		case 0:
			return std::numeric_limits<double>::quiet_NaN();
			break;
		case 1:
			return values[0];
			break;
		case 2:
			return (values[0] + values[1]) / 2.0;
			break;
		default:
			// WARNING: The following will irrevocably change the order of vector elements!
			std::sort(values.begin(), values.end());
			if(sampleSize % 2 == 1) return values[sampleSize / 2];
			else return (values[sampleSize / 2 - 1] + values[sampleSize / 2]) / 2.0;
	}
}*/

// ===================================================================================
// Median
// ===================================================================================
// This implementation uses partial sorting such that the central element of the array
// is in the correct position and all elements below or above are <= or >= the central
// element. For odd arrays the central element itself is returned as the median, while
// for even arrays the value of (max(low) + mid) / 2 is returned.
// ===================================================================================
double median(const std::vector <double> array){return median(&array[0], array.size());}
double median(const double *array, const size_t size)
{
	if(size == 0) return std::numeric_limits<double>::quiet_NaN();
	std::vector <double> aux_array(array, array + size);
	std::nth_element(aux_array.begin(), aux_array.begin() + (size / 2), aux_array.end());
	if(size % 2) return aux_array[size / 2];
	return (*std::max_element(aux_array.begin(), aux_array.begin() + (size / 2)) + aux_array[size / 2]) / 2.0;
}


// ===================================================================================
// Median absolute deviation (MAD)
// ===================================================================================
// This function derives the median absolute deviation about either the median or a
// user-specified value.
// ===================================================================================
double median_absolute_deviation(const std::vector <double> array, double value){return median_absolute_deviation(&array[0], array.size(), value);}
double median_absolute_deviation(const double *array, const size_t size, double value)
{
	if(size == 0) return std::numeric_limits<double>::quiet_NaN();
	if(std::isnan(value)) value = median(array, size);
	std::vector <double> aux_array(array, array + size);
	for(size_t i = 0; i < size; ++i) aux_array[i] = std::abs(aux_array[i] - value);
	return median(aux_array);
}
