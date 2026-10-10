// Compiler: MSVC v143 (Visual Studio 2022)
#include <iostream>
#include <vector>
#include <thread>
#include <algorithm>
#include <random>
#include <chrono>
#include <execution>
#include <fstream>
#include <format>
#include <functional>
#include <string>



const int kRepeatCount = 3;
const int kMinimumValue = 0;
const int kMaximumValue = 1000000;
const int kRandomSeed = 2024;
const std::size_t kSequenceSizes[] = { 2000000, 1000000, 5000000 };


std::string getFileName(std::size_t sequenceLength) {
    return "sequence_" + std::to_string(sequenceLength) + ".txt";
}
// Input file format: integer N - the total number of elements, N integers - the sequence data
void saveSequence(std::size_t sequenceLength, const std::vector<int>& inputVector) {
    std::ofstream dataFile(getFileName(sequenceLength));
    dataFile << sequenceLength << '\n';
    for (int number : inputVector) {
        dataFile << number << ' ';
    }
    dataFile.close();
}

std::vector<int> generateSequence(std::size_t sequenceLength, int seedValue) {
    std::mt19937 randomEngine(seedValue);
    std::uniform_int_distribution<int> numberDistribution(kMinimumValue, kMaximumValue);
    std::vector<int> generatedVector(sequenceLength);
    for (int& number : generatedVector) {
        number = numberDistribution(randomEngine);
    }
    return generatedVector;
}

void generateDataFiles() {
    int currentSeed = kRandomSeed;
    for (auto size : kSequenceSizes) {
        saveSequence(size, generateSequence(size, ++currentSeed));
    }
}

std::vector<int> loadSequence(std::size_t sequenceLength) {
    std::ifstream dataFile(getFileName(sequenceLength));
    std::size_t fileLength;
    dataFile >> fileLength;
    std::vector<int> loadedVector(fileLength);
    for (int& number : loadedVector) {
        dataFile >> number;
    }
    dataFile.close();
    return loadedVector;
}

std::vector<std::vector<int>> loadAllSequences() {
    std::vector<std::vector<int>> allDatasets;
    for (auto size : kSequenceSizes) {
        allDatasets.push_back(loadSequence(size));
    }
    return allDatasets;
}


std::size_t calculateParts(std::size_t dataSize, std::size_t targetParts) {
    if (dataSize == 0) {
        return 1;
    }
    return std::min(std::max(std::size_t(1), targetParts), dataSize);
}

void fillBounds(std::vector<std::size_t>& boundsVector, std::size_t dataSize, std::size_t actualParts) {
    for (std::size_t partIndex = 0, currentPosition = 0; partIndex < actualParts; ++partIndex) {

        currentPosition += (dataSize / actualParts) + (partIndex < (dataSize % actualParts) ? 1 : 0);

        boundsVector.push_back(currentPosition);
    }
}

std::vector<std::size_t> makeBounds(std::size_t dataSize, std::size_t requestedParts) {
    std::vector<std::size_t> boundsVector;
    boundsVector.push_back(0);
    fillBounds(boundsVector, dataSize, calculateParts(dataSize, requestedParts));
    return boundsVector;
}

void findMinWorker(
    std::vector<int>::const_iterator chunkBegin,

    std::vector<int>::const_iterator chunkEnd,

    std::vector<int>::const_iterator& resultIterator
) {
    resultIterator = std::min_element(chunkBegin, chunkEnd);
}


void executeSpawn(std::vector<std::thread>& threadPool,
    std::vector<int>::const_iterator chunkBegin,
    std::vector<int>::const_iterator chunkEnd,
    std::vector<int>::const_iterator& resultIterator) {
    threadPool.emplace_back(findMinWorker, chunkBegin, chunkEnd, std::ref(resultIterator));
   
}

void spawnThreads(std::vector<int>::const_iterator dataBegin,
    const std::vector<std::size_t>& boundsVector,
    std::vector<std::thread>& threadPool,
    std::vector<std::vector<int>::const_iterator>& localResults) {
    for (std::size_t chunkIndex = 0; chunkIndex < localResults.size(); ++chunkIndex) {
        executeSpawn(threadPool,
            dataBegin + boundsVector[chunkIndex],
            dataBegin + boundsVector[chunkIndex + 1],
            localResults[chunkIndex]);
    }
}


void joinThreads(std::vector<std::thread>& threadPool) {
    for (auto& currentThread : threadPool) {
        currentThread.join();
    }
}


std::vector<int>::const_iterator findGlobalMinimum(const std::vector<std::vector<int>::const_iterator>& localResults) {
    auto globalMinimumIterator = localResults[0];
    for (std::size_t resultIndex = 1; resultIndex < localResults.size(); ++resultIndex) {
        if (*localResults[resultIndex] < *globalMinimumIterator) {
            globalMinimumIterator = localResults[resultIndex];
        }
    }
    return globalMinimumIterator;
}

std::vector<int>::const_iterator parallelMinElement(
    std::vector<int>::const_iterator dataBegin,

    std::vector<int>::const_iterator dataEnd,
    std::size_t requestedParts) {

    auto boundsVector = makeBounds(dataEnd - dataBegin, requestedParts);
    std::vector<std::vector<int>::const_iterator> localResults(boundsVector.size() - 1);
    std::vector<std::thread> threadPool;

    spawnThreads(dataBegin, boundsVector, threadPool, localResults);
    joinThreads(threadPool);

    return findGlobalMinimum(localResults);
}



double measureTime(const std::function<std::vector<int>::const_iterator()>& testFunction) {
    double bestTimeMs = 0.0;

    for (int repeatIndex = 0; repeatIndex < kRepeatCount; ++repeatIndex) {
        auto startTime = std::chrono::steady_clock::now();
        testFunction();
        auto endTime = std::chrono::steady_clock::now();

        double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        bestTimeMs = (repeatIndex == 0) ? elapsedMs : std::min(bestTimeMs, elapsedMs);
    }
    return bestTimeMs;
}


void printLibraryHeader() {
    std::cout << "\nLibrary min_element, ms\n";
    std::cout << std::format("{:<10}{:<10}{:<10}{:<10}{:<10}{:<10}\n",
        "N", "plain", "seq", "unseq", "par", "par_unseq");
}

void printLibraryRow(std::size_t sequenceLength, const std::vector<double>& measuredTimes) {
    
    std::cout << std::format("{:<10}", sequenceLength);


    for (double timeMs : measuredTimes) {
        std::cout << std::format("{:<10.3f}", timeMs);
    }
    std::cout << std::endl;
}

void runLibraryTests(const std::vector<int>& dataVector) {
    auto dataBegin = dataVector.cbegin();
    auto dataEnd = dataVector.cend();
    std::vector<double> measuredTimes;

    measuredTimes.push_back(measureTime([&] { return std::min_element(dataBegin, dataEnd); }));
    measuredTimes.push_back(measureTime([&] { return std::min_element(std::execution::seq, dataBegin, dataEnd); }));
    measuredTimes.push_back(measureTime([&] { return std::min_element(std::execution::unseq, dataBegin, dataEnd); }));
    measuredTimes.push_back(measureTime([&] { return std::min_element(std::execution::par, dataBegin, dataEnd); }));
    measuredTimes.push_back(measureTime([&] { return std::min_element(std::execution::par_unseq, dataBegin, dataEnd); }));

    printLibraryRow(dataVector.size(), measuredTimes);
}


void printCustomHeader() {
    std::cout << "\nCustom parallel min_element, ms\n";
    std::cout << std::format("{:<6}", "K");

    for (auto size : kSequenceSizes) {
        std::cout << std::format("{:<10}", std::format("N={}", size));
    }
    std::cout << std::endl;
}

void printCustomRow(std::size_t threadCount, const std::vector<double>& measuredTimes) {
    
    std::cout << std::format("{:<6}", threadCount);

    
    for (double timeMs : measuredTimes) {
        std::cout << std::format("{:<10.3f}", timeMs);
    }
    std::cout << std::endl;
}

void printBestK(std::size_t sequenceLength, std::size_t bestThreadCount) {
    double hardwareThreads = std::thread::hardware_concurrency();
    std::cout << "N=" << sequenceLength << "  K=" << bestThreadCount << "  K/HW=";
    std::cout << std::fixed << std::setprecision(2)<< (hardwareThreads > 0 ? (bestThreadCount / hardwareThreads) : 0) << '\n';
}

void evaluateK(
    std::size_t threadCount,

    const std::vector<std::vector<int>>& allDatasets,

    std::vector<std::vector<double>>& allMeasuredTimes) {

    std::vector<double> rowTimes;
    for (const auto& dataset : allDatasets) {
        rowTimes.push_back(measureTime([&] {
            return parallelMinElement(dataset.cbegin(), dataset.cend(), threadCount);
            }));
    }
    printCustomRow(threadCount, rowTimes);
    allMeasuredTimes.push_back(rowTimes);
}

void calculateAndPrintBest(const std::vector<std::size_t>& threadCountList,

    const std::vector<std::vector<double>>& allMeasuredTimes) {
    std::cout << "\nBest K analysis (Hardware threads=" << std::thread::hardware_concurrency() << ")\n";

    for (std::size_t columnIndex = 0; columnIndex < 3; ++columnIndex) {

        std::size_t bestRowIndex = 0;

        for (std::size_t rowIndex = 1; rowIndex < allMeasuredTimes.size(); ++rowIndex) {

            if (allMeasuredTimes[rowIndex][columnIndex] < allMeasuredTimes[bestRowIndex][columnIndex]) {
                bestRowIndex = rowIndex;
            }
        }
        printBestK(kSequenceSizes[columnIndex], threadCountList[bestRowIndex]);
    }
}

void runAllCustomTests(const std::vector<std::vector<int>>& allDatasets) {
    printCustomHeader();

    std::vector<std::size_t> threadCountList;

    unsigned int hardwareThreads = std::thread::hardware_concurrency();

    std::size_t maximumK = (hardwareThreads > 0) ? std::min<std::size_t>(hardwareThreads, 16) : 16;

    for (std::size_t threadCount = 1; threadCount <= maximumK; ++threadCount) {
        threadCountList.push_back(threadCount);
    }

    std::vector<std::vector<double>> allMeasuredTimes;

    for (std::size_t threadCount : threadCountList) {
        evaluateK(threadCount, allDatasets, allMeasuredTimes);
    }

    calculateAndPrintBest(threadCountList, allMeasuredTimes);
}


int main() {
    
      generateDataFiles();
      auto allDatasets = loadAllSequences();

      printLibraryHeader();
      for (const auto& dataset : allDatasets) {
          runLibraryTests(dataset);
      }

      runAllCustomTests(allDatasets);
    
}