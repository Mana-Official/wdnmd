
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <time.h>
#include <DigitalType.h>
#include <WaveformReaderForCompetition.h>

std::string varTypeToString(int type) {
    if (type == REG)
        return "reg";
    else if (type == UWIRE)
        return "uwire";
    else if (type == WIRE)
        return "wire";
    else if (type == TRI)
        return "tri";
    else if (type == TRI1)
        return "tri1";
    else if (type == SUPPLY0)
        return "supply0";
    else if (type == WAND)
        return "wand";
    else if (type == TRIAND)
        return "triand";
    else if (type == TRI0)
        return "tri0";
    else if (type == SUPPLY1)
        return "supply1";
    else if (type == WOR)
        return "wor";
    else if (type == TRIOR)
        return "trior";
    else if (type == TRIREG)
        return "trireg";
    else if (type == INTEGER)
        return "integer";
    else if (type == PARAMETER)
        return "parameter";
    else if (type == REAL)
        return "real";
    else if (type == TIME)
        return "time";
    else if (type == EVENT)
        return "event";

    return std::string();
}

void getInputFile(int argc, const char** argv, std::string& inputFile) {
    for(int i = 1; i < argc; ++i) {
        if(argv[i][0] != '-') {
            inputFile = argv[i];
            break;
        }
    }
    assert(!inputFile.empty());
}

void dumpScopeInfo(Scope* scope, FILE* fp) {
    assert(scope);
    fprintf(fp, "$scope %s %s $end\n", scope->type().c_str(), scope->name().c_str());
    const std::map<std::string, std::string>& vars = scope->getVars();
    std::map<std::string, std::string>::const_iterator it;
    for (it = vars.cbegin(); it != vars.cend(); ++it) {
        std::string varName = (*it).first;
        std::string symbol = (*it).second;
        std::string type = varTypeToString(scope->getSignalType(varName));
        int width = scope->getSignalBus(varName);
        fprintf(fp, "$var %s %d %s %s $end\n", type.data(), width, symbol.data(),
                varName.data());
    }
    // get sub socpes info
    const std::vector<Scope*> &subscopes = scope->getSubScopes();
    for (unsigned i = 0; i < subscopes.size(); ++i) {
        dumpScopeInfo(subscopes[i], fp);
    }
}

void dumpTimeVCDatas(FILE *pOut, long long time,
        const std::vector<std::string> &vcDatas) {
    fprintf(pOut, "#%lld\n", time);
    for (unsigned j = 0; j < vcDatas.size(); ++j) {
        fprintf(pOut, "%s\n", vcDatas[j].c_str()); 
    }
}

int main(int argc, const char** argv) {
    if (argc < 2) {
        printf("Please input a VCD waveform file\n");
        exit(-1);
    }

    bool dump = false;
    for(int i = 1; i < argc; ++i) {
        std::string argvs = argv[i];
        if (argvs.find("-dump") != std::string::npos) {
            dump = true;
        }
    }

    std::string inputFile;
    getInputFile(argc, argv, inputFile);
    WaveformReaderForCompetition reader(inputFile);
    std::string outFile = inputFile + ".out";
    FILE* pOut = nullptr;
    if (dump) {
        pOut = fopen(outFile.data(), "w");
        if (pOut == nullptr) {
            std::cout << " cant' open file " << outFile << std::endl;
        }
    }

    // read header.
    clock_t start, finish;
    start = clock();
    reader.readHeader(); // header include data, version, time scale, and var definition
    if (pOut) {
        fprintf(pOut, "$date\n %s\n$end\n", reader.getDate().data());
        fprintf(pOut, "$version\n %s\n$end\n", reader.getVersion().data());
        fprintf(pOut, "$timescale\n %s\n $end\n", reader.getTimeScale().data());

        std::vector<Scope*> topscopes;
        reader.topScopes(topscopes);
        for (unsigned i = 0; i < topscopes.size(); ++i) {
            dumpScopeInfo(topscopes[i], pOut);
        }
        fprintf(pOut, "$enddefinitions $end\n\n");
    }
    //read all signals time block data.

    std::map<char, unsigned char> scalar_converter;
    scalar_converter['0'] = 0;
    scalar_converter['1'] = 1;
    scalar_converter['x'] = 2;
    scalar_converter['z'] = 3;
    std::vector<std::vector<std::pair<long long, unsigned char>>> scalar_data; // i for signal, j for time
    std::vector<std::vector<std::pair<long long, std::vector<unsigned char>>>> vector_data; // i for signal, j for time
    std::map<std::string, unsigned> data_type;    // 0 for scalar, 1 for vector, 2 for real
    std::map<std::string, unsigned> scalar_index;
    std::map<std::string, unsigned> vector_index;

    reader.beginReadTimeData(); //must call this function before read VC data.
    while (!reader.isDataFinished()) {
        reader.readNextPointData();
        long long time = reader.getVCTime();
        const std::vector<std::string> &vcDatas = reader.getVCValues();
        // process data here.

        for (std::string s : vcDatas) {
            unsigned l = s.length();
            unsigned index;
            // if (isdigit(s[0]) || s[0] == 'x' || s[0] == 'z') {    // scalar
            if (scalar_converter.count(s[0])) { // scalar
                std::string name = s.substr(1, l); // s[1:] (correct code)
                if (!scalar_index.count(name)) {    // not inserted
                    scalar_data.push_back(std::vector<std::pair<long long, unsigned char>>(0));
                    index = scalar_data.size() - 1;
                    data_type[name] = 0;
                    scalar_index[name] = index;
                } else
                    index = scalar_index[name];
                scalar_data[index].push_back(std::make_pair(time, scalar_converter[s[0]]));
            } else if (s[0] == 'b') {   // vector
                unsigned space_pos = s.length()-1;
                while (s[space_pos] != ' ') space_pos--;
                std::string name = s.substr(space_pos+1, 2);
                std::string bits = s.substr(1, space_pos-1);
                if (!vector_index.count(name)) {    // not inserted
                    vector_data.push_back(std::vector<std::pair<long long, std::vector<unsigned char>>>(0));
                    index = vector_data.size() - 1;
                    data_type[name] = 1;
                    vector_index[name] = index;
                } else
                    index = vector_index[name];
                vector_data[index].push_back(std::make_pair(time, std::vector<unsigned char>(0)));
                vector_data[index].back().second.reserve(bits.length());
                for (auto b : bits)
                    vector_data[index].back().second.push_back(scalar_converter[b]);
            }
        }

        if (pOut) {
            dumpTimeVCDatas(pOut, time, vcDatas);
        }
    }
    finish = clock();
    if (pOut) {
        std::cout<< "ReadAndOutTime: " << (finish - start) / CLOCKS_PER_SEC << std::endl;
        fclose(pOut);
    } else {
        std::cout<< "ReadTime: " << (finish - start) / CLOCKS_PER_SEC << std::endl;
    }

    // if read specified sigs time block datas. 
    /*
    std::set<std::string> sigSymbols;
    reader.read(sigSymbols);
    while (!reader.isDataFinished()) {
        reader.readNextPointData();
        long long time = reader.getVCTime();
        const std::vector<std::string> &vcDatas = reader.getVCValues();
        // process data here.
        if (pOut) {
            dumpTimeVCDatas(pOut, time, vcDatas);
        }
    }
    */
}
