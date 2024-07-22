#ifndef PARALLAXGENPLUGIN_H
#define PARALLAXGENPLUGIN_H

#include <string>

#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

#include "XEditLibCpp/XEditLibCpp.hpp"

class ParallaxGenPlugin
{
private:
    XEditLibCpp xelib;

    ParallaxGenDirectory* pgd;

    inline static const std::wstring MDL_RECORDS = L"ACTI,ADDN,ALCH,AMMO,ANIO,APPA,ARMA,ARMO,ARTO,BOOK,BPTD,CAMS,CLMT,CONT,DEBR,DOOR,EXPL,FLOR,FURN,GRAS,HAZD,HDPT,INGR,IPCT,KEYM";

public:
    ParallaxGenPlugin(ParallaxGenDirectory* pgd);

    std::vector<std::wstring> getActivePlugins();
    void createTXSTPatch();

private:
    unsigned int* getTXSTRecords();
    void buildNifMap();
};

#endif
