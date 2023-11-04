#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <iterator>
#include <algorithm>
#include <boost\algorithm\string.hpp>
#include <glob.h>
#include <fstream>
#include <dirent.h>
#include <sys/types.h>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include <map>
#include <zlib.h>
#include "porter2_stemmer.h"
using namespace std;
using namespace boost;

string stopWords[] = {
    "ourselves", "hers", "between", "yourself", "but", "again", "there", "about", "once", "during", "out", "very", "having",
    "with", "they", "own", "an", "be", "some", "for", "do", "its", "yours", "such", "into", "of", "most", "itself", "other", "off", "is", "s",
    "am", "or", "who", "as", "from", "him", "each", "the", "themselves", "until", "below", "are", "we", "these", "your", "his", "through", "don",
    "nor", "me", "were", "her", "more", "himself", "this", "down", "should", "our", "their", "while", "above", "both", "up", "to", "ours", "had",
    "she", "all", "no", "when", "at", "any", "before", "them", "same", "and", "been", "have", "in", "will", "on", "does", "yourselves", "then",
    "that", "because", "what", "over", "why", "so", "can", "did", "not", "now", "under", "he", "you", "herself", "has", "just", "where", "too",
    "only", "myself", "which", "those", "i", "after", "few", "whom", "t", "being", "if", "theirs", "my", "against", "a", "by", "doing", "it",
    "how", "further", "was", "here", "than"};

int sizeStop = sizeof(stopWords) / sizeof(stopWords[0]);

bool isStopWord(string s)
{
    for (int i = 0; i < sizeStop; i++)
    {
        if (s == stopWords[i])
            return true;
    }
    return false;
}

std::wstring my_to_wstring(const std::string &s)
{
    std::wstring temp(s.length(), L' ');
    std::copy(s.begin(), s.end(), temp.begin());
    return temp;
}

std::string my_to_string(const std::wstring &s)
{
    std::string temp(s.length(), ' ');
    std::copy(s.begin(), s.end(), temp.begin());
    return temp;
}

std::string performStemming(const std::string &input)
{
    std::string stemmedText = input;
    Porter2Stemmer::trim(stemmedText); // Optionally, trim leading and trailing whitespace
    Porter2Stemmer::stem(stemmedText);
    return stemmedText;
}

int createSortedRuns(string inputFileName, string sortedRunName, int nFilesSort)
{
    const char *fileName = inputFileName.c_str();
    ifstream file(fileName);
    // in pair, first is the file no, and second is the no of occurences of the term in that particular file
    map<string, vector<pair<int, int>>> vocab;
    map<string, vector<pair<int, int>>>::iterator it;
    pair<int, int> tempPair;
    tempPair.first = 0;
    tempPair.second = 1;
    vector<pair<int, int>> tempVec;
    tempVec.push_back(tempPair);

    // Read file
    int i;
    int j = 0;
    int fileNo;
    vector<string> splitVec;
    bool newFile = true;
    if (file.is_open())
    {
        string line;
        ofstream sortedRun;
        while (getline(file, line))
        {
            trim(line);
            split(splitVec, line, is_any_of(" "), token_compress_on);
            if (splitVec.size() < 2)
                continue;
            // cout << splitVec.size() << endl;

            i = stoi(splitVec[1]);
            if (i % nFilesSort == 1 && newFile)
            {
                if (sortedRun.is_open())
                {
                    for (it = vocab.begin(); it != vocab.end(); it++)
                    {
                        sortedRun << it->first;
                        for (int z = 0; z < it->second.size(); z++)
                        {
                            sortedRun << " " << it->second[z].first << " " << it->second[z].second;
                        }
                        sortedRun << endl;
                    }
                    sortedRun.close();
                }
                j++;
                vocab.clear();
                sortedRun.open((sortedRunName + to_string(j)).c_str());
                newFile = false;
            }
            else if (i % nFilesSort != 1)
            {
                newFile = true;
            }
            it = vocab.find(splitVec[0]);
            if (it != vocab.end())
            {
                fileNo = it->second[it->second.size() - 1].first;
                if (fileNo == i)
                {
                    it->second[it->second.size() - 1].second++;
                }
                else
                {
                    tempPair.first = i;
                    it->second.push_back(tempPair);
                }
            }
            else
            {
                tempVec[0].first = i;
                vocab.insert(make_pair(splitVec[0], tempVec));
            }
        }
        file.close();
        if (sortedRun.is_open())
        {
            for (it = vocab.begin(); it != vocab.end(); it++)
            {
                sortedRun << it->first;
                for (int z = 0; z < it->second.size(); z++)
                {
                    sortedRun << " " << it->second[z].first << " " << it->second[z].second;
                }
                sortedRun << endl;
            }
            sortedRun.close();
        }
    }
    remove(inputFileName.c_str());
    return j;
}
struct WordInfo
{
    long long start; // Start position in the merged file
    long long end;   // End position in the merged file
    int docCount;    // Number of documents containing the word

    void WriteBinary(std::ofstream &file) const
    {
        file.write((char *)&start, sizeof(long long));
        file.write((char *)&end, sizeof(long long));
        file.write((char *)&docCount, sizeof(int));
    }
};
void WriteLexiconBinary(const std::string &filename, const std::map<std::string, WordInfo> &wordInfoMap)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open binary lexicon file: " << filename << std::endl;
        return;
    }

    for (const auto &pair : wordInfoMap)
    {
        int wordSize = pair.first.size();
        file.write((char *)&wordSize, sizeof(int));
        file.write(pair.first.c_str(), wordSize);
        pair.second.WriteBinary(file);
    }
}

void mergeAndCompressSortedRuns(string inputFileName, string mergedFileName, int totalSortedRuns)
{
    const char *fileName;
    vector<ifstream> files(totalSortedRuns);
    string line;
    vector<string> words;
    string word;
    vector<vector<string>> wordIndex;
    string outputLine;
    vector<string> tempWordIndex;

    for (int i = 1; i <= totalSortedRuns; i++)
    {
        fileName = (inputFileName + to_string(i)).c_str();
        files[i - 1].open(fileName);
        getline(files[i - 1], line);
        trim(line);
        split(tempWordIndex, line, is_any_of(" "), token_compress_on);
        words.push_back(tempWordIndex[0]);
        tempWordIndex.erase(tempWordIndex.begin());
        wordIndex.push_back(tempWordIndex);
    }

    int minIndex, index;
    ofstream compressedIndexFile(mergedFileName + ".bin", ios::binary);
    map<string, WordInfo> wordInfoMap;
    long long currentPosition = 0;

    while (files.size() > 0)
    {
        vector<int> merge;
        minIndex = 0;
        merge.push_back(0);
        for (int i = 1; i < words.size(); i++)
        {
            if (words[i] < words[minIndex])
            {
                merge.clear();
                merge.push_back(i);
                minIndex = i;
            }
            else if (words[i] == words[minIndex])
            {
                merge.push_back(i);
            }
        }

        outputLine = words[minIndex];
        vector<int> toErase;
        for (int i = 0; i < merge.size(); i++)
        {
            index = merge[i];
            for (int j = 0; j < wordIndex[index].size(); j++)
            {
                outputLine += " " + wordIndex[index][j];
            }
            getline(files[index], line);
            trim(line);
            if (line == "")
            {
                // remove
                toErase.push_back(index);
            }
            else
            {
                split(tempWordIndex, line, is_any_of(" "), token_compress_on);
                words[index] = tempWordIndex[0];
                tempWordIndex.erase(tempWordIndex.begin());
                wordIndex[index] = tempWordIndex;
            }
        }

        for (int i = toErase.size() - 1; i > -1; i--)
        {
            index = toErase[i];
            files.erase(files.begin() + index);
            words.erase(words.begin() + index);
            tempWordIndex.erase(tempWordIndex.begin() + index);
            wordIndex.erase(wordIndex.begin() + index);
        }
        for (int i = 0; i < merge.size(); i++)
        {
            index = merge[i];
            wordInfoMap[words[index]].end = currentPosition - 1; // The end of the previous word's list
            wordInfoMap[words[index]].docCount++;                // Increase the document count
        }
        ofstream lexiconFile("lexicon.txt");
        for (const auto &pair : wordInfoMap)
        {
            lexiconFile << pair.first << " " << pair.second.start << " " << pair.second.end << " " << pair.second.docCount << endl;
        }
        WriteLexiconBinary("lexicon.bin", wordInfoMap);
        lexiconFile.close();

        // Write the outputLine to the merged file

        // Update the current position

        // Apply compression (e.g., delta encoding) to the wordInfoMap before writing to the binary file

        int prevDocCount = 0;
        for (auto &pair : wordInfoMap)
        {
            int currentDocCount = pair.second.docCount;
            pair.second.docCount = currentDocCount - prevDocCount;
            prevDocCount = currentDocCount;
        }

        // Write the compressed wordInfoMap to the binary file

        currentPosition = compressedIndexFile.tellp();
    }

    // Close the compressed index file
    compressedIndexFile.close();

    // Remove the input files

    for (int i = 1; i <= totalSortedRuns; i++)
    {
        fileName = (inputFileName + to_string(i)).c_str();
        remove(fileName);
    }
}
void mergeSortedRunsAndCompress(string inputFileName, string mergedFileName, int totalSortedRuns)
{
    const char *fileName;
    vector<ifstream> files(totalSortedRuns);
    string line;
    vector<string> words;
    string word;
    vector<vector<string>> wordIndex;
    vector<string> tempWordIndex;

    for (int i = 1; i <= totalSortedRuns; i++)
    {
        fileName = (inputFileName + to_string(i)).c_str();
        files[i - 1].open(fileName);
        getline(files[i - 1], line);
        trim(line);
        split(tempWordIndex, line, is_any_of(" "), token_compress_on);
        words.push_back(tempWordIndex[0]);
        tempWordIndex.erase(tempWordIndex.begin());
        wordIndex.push_back(tempWordIndex);
    }

    int minIndex, index;
    ofstream compressedIndexFile(mergedFileName + ".bin", ios::binary);
    map<string, WordInfo> wordInfoMap;
    string outputLine;
    long long currentPosition = 0;

    while (files.size() > 0)
    {
        vector<int> merge;
        minIndex = 0;
        merge.push_back(0);
        for (int i = 1; i < words.size(); i++)
        {
            if (words[i] < words[minIndex])
            {
                merge.clear();
                merge.push_back(i);
                minIndex = i;
            }
            else if (words[i] == words[minIndex])
            {
                merge.push_back(i);
            }
        }

        outputLine = words[minIndex];
        vector<int> toErase;
        for (int i = 0; i < merge.size(); i++)
        {
            index = merge[i];
            for (int j = 0; j < wordIndex[index].size(); j++)
            {
                outputLine += " " + wordIndex[index][j];
            }
            getline(files[index], line);
            trim(line);
            if (line == "")
            {
                // remove
                toErase.push_back(index);
            }
            else
            {
                split(tempWordIndex, line, is_any_of(" "), token_compress_on);
                words[index] = tempWordIndex[0];
                tempWordIndex.erase(tempWordIndex.begin());
                wordIndex[index] = tempWordIndex;
            }
        }

        for (int i = toErase.size() - 1; i > -1; i--)
        {
            index = toErase[i];
            files.erase(files.begin() + index);
            words.erase(words.begin() + index);
            tempWordIndex.erase(tempWordIndex.begin() + index);
            wordIndex.erase(wordIndex.begin() + index);
        }
        for (int i = 0; i < merge.size(); i++)
        {
            index = merge[i];
            wordInfoMap[words[index]].end = currentPosition - 1; // The end of the previous word's list
            wordInfoMap[words[index]].docCount++;                // Increase the document count
        }
        ofstream lexiconFile("lexicon.txt");
        for (const auto &pair : wordInfoMap)
        {
            lexiconFile << pair.first << " " << pair.second.start << " " << pair.second.end << " " << pair.second.docCount << endl;
        }
        WriteLexiconBinary("lexicon.bin", wordInfoMap);
        lexiconFile.close();

        // Write the outputLine to the merged file

        // Update the current position

        // Apply compression (e.g., delta encoding) to the wordInfoMap before writing to the binary file

        int prevDocCount = 0;
        for (auto &pair : wordInfoMap)
        {
            int currentDocCount = pair.second.docCount;
            pair.second.docCount = currentDocCount - prevDocCount;
            prevDocCount = currentDocCount;
        }

        // Write the compressed wordInfoMap to the binary file

        currentPosition = compressedIndexFile.tellp();
    }

    // Close the compressed index file
    compressedIndexFile.close();

    // Remove the input files

    // ...

    for (int i = 1; i <= totalSortedRuns; i++)
    {
        fileName = (inputFileName + to_string(i)).c_str();
        remove(fileName);
    }
}

struct Document
{
    string docID;
    std::string URL;
    std::string title;
    std::string body;
};

// Function to parse a line and extract fields.
Document parseLine(const std::string &line)
{
    Document doc;
    std::istringstream iss(line);
    std::getline(iss, doc.docID, '\t');
    std::getline(iss, doc.URL, '\t');
    std::getline(iss, doc.title, '\t');
    std::getline(iss, doc.body);
    return doc;
}
struct PageInfo
{
    std::string URL;
    int size;

    void WriteBinary(std::ofstream &file) const
    {
        int urlSize = URL.size();
        file.write((char *)&urlSize, sizeof(int));
        file.write(URL.c_str(), urlSize);
        file.write((char *)&size, sizeof(int));
    }
};
void WritePageTableBinary(const std::string &filename, const std::vector<PageInfo> &pageInfo)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open binary page table file: " << filename << std::endl;
        return;
    }

    for (const PageInfo &info : pageInfo)
    {
        info.WriteBinary(file);
    }
}

int main()
{
    time_t start, end;
    time(&start);
    vector<string> a;
    std::string inputFilePath = "D:\\dataset\\msmarco-docs.tsv";
    gzFile inputFile = gzopen(InputFilePath.c_str(), "rb");
    if (!inputFile)
    {
        std::cerr << "Failed to open input file: " << InputFilePath << std::endl;
        return 1;
    }
    char buffer[8192]; // Adjust the buffer size as needed
    std::string pageTableFileName = "page_table.txt";
    // Open the input file
    std::ifstream inputFile(inputFilePath);
    string outputFileName = "unsorted";
    ofstream outputFile(outputFileName);
    if (!inputFile.is_open())
    {
        std::cerr << "Failed to open input file: " << inputFilePath << std::endl;
        return 1;
    }

    std::string line;
    int count_doc = 0;
    vector<PageInfo> pageInfo;
    string sortedRunName = "sortedRun";
    int totalSortedRuns = 0;
    while (gzgets(inputFile, buffer, sizeof(buffer)))
    {
        if (count_doc == 100)
        {
            break;
        }

        // Decompress the line
        uLong length = sizeof(buffer);
        char decompressedLine[8192]; // Adjust the buffer size as needed
        int ret = uncompress((Bytef *)decompressedLine, &length, (const Bytef *)buffer, strlen(buffer));

        // Check for errors in decompression
        if (ret != Z_OK)
        {
            std::cerr << "Decompression error: " << ret << std::endl;
            return 1;
        }

        // Process the decompressed line to extract document information
        Document doc = parseLine(decompressedLine);

        // Create an intermediate posting format and write to the temporary file

        PageInfo pageInfoEntry;
        pageInfoEntry.URL = doc.URL;
        std::istringstream iss(doc.body);
        std::string word;
        int termCount = 0;

        while (iss >> word)
        {
            string stemmedString = performStemming(word);

            // Write the stemmed string and document ID to the output file
            outputFile << stemmedString << " " << count_doc << "\n";
            termCount++;
        }
        pageInfoEntry.size = termCount;

        pageInfo.push_back(pageInfoEntry);
        count_doc++;
        std::ofstream pageTableFile(pageTableFileName);
        for (int i = 0; i < pageInfo.size(); i++)
        {
            pageTableFile << i << " " << pageInfo[i].URL << " " << pageInfo[i].size << "\n";
        }
        pageTableFile.close();
        // createUnsortedVocab(outputFileName, a);
        // cout << "Unsorted Vocabulary created" << endl;

        string sortedRunName = "sortedRun";
        int nFilesSort = 50;
        totalSortedRuns = totalSortedRuns + createSortedRuns(outputFileName, sortedRunName, nFilesSort);
        // cout << "Sorted Runs created" << endl;
    }
    // cout << a[0];
    string mergedFileName = "finalIndex";
    mergeSortedRunsAndCompress(sortedRunName, mergedFileName, totalSortedRuns);

    cout << "Megred Runs. Inverted Index created" << endl;
    WritePageTableBinary("page_table.bin", pageInfo);

    time(&end);
    double time_taken = double(end - start);
    cout << "Time taken by program is : " << fixed
         << time_taken << setprecision(5);
    cout << " sec " << endl;
    return 0;
}
