// Golden-file test: td1-core vs the JS prototype reference.
// usage: td1_golden <dir containing manifest.txt>
#include "td1/Kernel.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static bool readPlanar(const std::string& file, size_t n, std::vector<float>& L, std::vector<float>& R) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    L.resize(n); R.resize(n);
    f.read(reinterpret_cast<char*>(L.data()), (std::streamsize)(n * sizeof(float)));
    f.read(reinterpret_cast<char*>(R.data()), (std::streamsize)(n * sizeof(float)));
    return (bool)f;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: td1_golden <dir>\n"); return 2; }
    const std::string dir = argv[1];
    std::ifstream man(dir + "/manifest.txt");
    if (!man) { std::fprintf(stderr, "no manifest in %s (run render_reference.mjs)\n", dir.c_str()); return 2; }

    // Tolerance: the reference keeps state in doubles like we do; residual
    // differences come from float storage order and libm. -100 dBFS is far
    // below audibility and far above what any real porting bug produces.
    const double kTol = 1e-5;
    int failures = 0, cases = 0;
    double worst = 0; std::string worstName;
    std::string line;
    while (std::getline(man, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string name, inFile, refFile; double sr; size_t n;
        ss >> name >> sr >> n >> inFile >> refFile;
        std::map<std::string, double> kv;
        std::string tok;
        while (ss >> tok) {
            const auto eq = tok.find('=');
            kv[tok.substr(0, eq)] = std::stod(tok.substr(eq + 1));
        }
        std::vector<float> iL, iR, rL, rR;
        if (!readPlanar(dir + "/" + inFile, n, iL, iR) || !readPlanar(dir + "/" + refFile, n, rL, rR)) {
            std::fprintf(stderr, "FAIL %s: could not read data\n", name.c_str()); ++failures; continue;
        }

        td1::Params p;
        p.attackDb = (float)kv["attack"]; p.sustainDb = (float)kv["sustain"];
        p.speed = (float)kv["speed"]; p.hpfHz = (float)kv["schpf"];
        p.tiltDb = (float)kv["tilt"]; p.pivotHz = (float)kv["pivot"];
        p.lookaheadMs = (float)kv["lookMs"]; p.mix = (float)kv["mix"];
        p.outputDb = (float)(20.0 * std::log10(kv["out"]));
        p.clip = kv["clip"] != 0; p.link = kv["link"] != 0; p.delta = kv["delta"] != 0;
        p.bypass = kv["bypass"] != 0; p.autoGain = kv["autoGain"] != 0; p.audition = kv["audition"] != 0;
        p.target = (td1::Target)(int)kv["msMode"];

        td1::Kernel k;
        k.setParams(p);
        k.prepare(sr);
        // The reference applies `out` as a linear factor; we take dB and convert
        // back. Exactness at rest is preserved up to pow/log10 round trip.

        std::vector<float> oL(n), oR(n);
        // Irregular block sizes: the kernel must be block-size invariant.
        static const int kBlocks[] = { 1, 7, 64, 511, 1024, 33, 4096 };
        size_t pos = 0; int bi = 0;
        while (pos < n) {
            const int m = (int)std::min<size_t>((size_t)kBlocks[bi++ % 7], n - pos);
            k.process(iL.data() + pos, iR.data() + pos, oL.data() + pos, oR.data() + pos, m);
            pos += (size_t)m;
        }

        double maxErr = 0; size_t at = 0;
        for (size_t i = 0; i < n; ++i) {
            const double e = std::max(std::abs((double)oL[i] - rL[i]), std::abs((double)oR[i] - rR[i]));
            if (e > maxErr) { maxErr = e; at = i; }
        }
        ++cases;
        if (maxErr > worst) { worst = maxErr; worstName = name; }
        if (maxErr > kTol) {
            ++failures;
            std::fprintf(stderr, "FAIL %-28s max err %.3g at sample %zu\n", name.c_str(), maxErr, at);
        }
    }
    std::printf("golden: %d/%d cases within %.0e (worst %.3g in %s)\n",
                cases - failures, cases, kTol, worst, worstName.c_str());
    return failures == 0 && cases > 0 ? 0 : 1;
}
