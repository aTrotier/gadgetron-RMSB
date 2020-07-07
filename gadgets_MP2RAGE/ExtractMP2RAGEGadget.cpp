/*
 *
 * ExtractMagnitudeGadget.cpp
 *
 *  Created on: Nov 8, 2011
 *      Author: Michael S. Hansen
 */

// #include "Temps.h"

#include <bitset>
#include <unordered_map>
#include <gadgetron/hoNDArray_math.h>
#include <gadgetron/mri_core_def.h>

#include "ExtractMP2RAGEGadget.h"
#include <boost/math/constants/constants.hpp>

namespace Gadgetron {

namespace {
using IMTYPE = ISMRMRD::ISMRMRD_ImageTypes;


static const std::unordered_map<IMTYPE,std::function<float(std::complex<float>)>> extract_functions = {
{IMTYPE::ISMRMRD_IMTYPE_MAGNITUDE, [](std::complex<float> v) { return std::abs(v); }},
{IMTYPE::ISMRMRD_IMTYPE_REAL,[](std::complex<float> v) { return std::real(v); }},
{IMTYPE::ISMRMRD_IMTYPE_IMAG,[](std::complex<float> v) { return std::imag(v); }},
{IMTYPE::ISMRMRD_IMTYPE_PHASE,[](std::complex<float> v) { return std::arg(v)+boost::math::constants::pi<float>(); }}
};

static const std::unordered_map<IMTYPE,size_t> series_offset{
    {IMTYPE::ISMRMRD_IMTYPE_MAGNITUDE,0} ,
    {IMTYPE::ISMRMRD_IMTYPE_REAL,1000},
    {IMTYPE::ISMRMRD_IMTYPE_IMAG,2000},
    {IMTYPE::ISMRMRD_IMTYPE_PHASE,3000}};
}

ExtractMP2RAGEGadget::ExtractMP2RAGEGadget() {


}

ExtractMP2RAGEGadget::~ExtractMP2RAGEGadget() {

}

int ExtractMP2RAGEGadget::process_config(ACE_Message_Block *mb) {

    if (int(extract_mask) > 0){
        const auto bitmask = std::bitset<4>(int(extract_mask));
        for (int imtype = IMTYPE::ISMRMRD_IMTYPE_MAGNITUDE; imtype <= IMTYPE::ISMRMRD_IMTYPE_PHASE; imtype++){
            if (bitmask[imtype-1]) image_types.push_back(IMTYPE(imtype));
        }
    } else {
        if (extract_magnitude) image_types.push_back(IMTYPE::ISMRMRD_IMTYPE_MAGNITUDE);
        if (extract_real) image_types.push_back(IMTYPE::ISMRMRD_IMTYPE_REAL);
        if (extract_imag) image_types.push_back(IMTYPE::ISMRMRD_IMTYPE_IMAG);
        if (extract_phase) image_types.push_back(IMTYPE::ISMRMRD_IMTYPE_PHASE);
    }

    if (image_types.empty()) throw std::runtime_error("ExtractMP2RAGEGadget: No valid extract functions specified");

    return GADGET_OK;
}

float ExtractMP2RAGEGadget::minimum_component(const Gadgetron::hoNDArray<std::complex<float>> & image) {


    float* float_start = (float*)image.get_data_ptr();
    float* float_end = (float*) image.end();

    return *std::min_element(float_start,float_end);
}

int ExtractMP2RAGEGadget::process(GadgetContainerMessage<ISMRMRD::ImageHeader> *m1,
                                  GadgetContainerMessage<hoNDArray<std::complex<float>>> *m2) {


    float min_val = minimum_component(*m2->getObjectPtr());

    for (IMTYPE image_type : image_types) {


        GadgetContainerMessage<ISMRMRD::ImageHeader> *cm1 =
                new GadgetContainerMessage<ISMRMRD::ImageHeader>();

        Gadgetron::GadgetContainerMessage<ISMRMRD::MetaContainer>* cm3 = new Gadgetron::GadgetContainerMessage<ISMRMRD::MetaContainer>();

        //Copy the header
        *cm1->getObjectPtr() = *m1->getObjectPtr();

        auto cm2 = new GadgetContainerMessage<hoNDArray<float> >(m2->getObjectPtr()->get_dimensions());

        std::complex<float> *src = m2->getObjectPtr()->get_data_ptr();
        float *dst = cm2->getObjectPtr()->get_data_ptr();


        for (unsigned long i = 0; i < cm2->getObjectPtr()->get_number_of_elements(); i++) {
            dst[i] = extract_functions.at(image_type)(src[i]);
        }

        if (real_imag_offset != 0 && (image_type == IMTYPE::ISMRMRD_IMTYPE_REAL || image_type == IMTYPE::ISMRMRD_IMTYPE_IMAG)) {
            *cm2->getObjectPtr() += float(real_imag_offset);
        }

        cm1->cont(cm2);
        cm2->cont(cm3);

        cm1->getObjectPtr()->data_type = ISMRMRD::ISMRMRD_FLOAT;//GADGET_IMAGE_REAL_FLOAT;
        cm1->getObjectPtr()->image_type = image_type;

        if (cm1->getObjectPtr()->image_series_index==1000)
        {
          //  std::cout <<"Cas où index = 1000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "Magn_TI1_Under");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "Magn_TI1_Under");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");

        }
        else if (cm1->getObjectPtr()->image_series_index==2000)
        {
          //  std::cout <<"Cas où index = 2000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "Magn_TI2_Under");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "Magn_TI2_Under");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");

        }
        else if (cm1->getObjectPtr()->image_series_index==3000)
        {
          //  std::cout <<"Cas où index = 3000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "MP2RAGE_Under");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "MP2RAGE_Under");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");

        }
        else if (cm1->getObjectPtr()->image_series_index==4000)
        {
          //  std::cout <<"Cas où index = 4000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_DATA_ROLE, GADGETRON_IMAGE_T1MAP);
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "T1_Under");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "T1_Under");
            //cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, GADGETRON_IMAGE_T1MAP);
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");
            cm3->getObjectPtr()->set(GADGETRON_IMAGE_COLORMAP, "Perfusion.pal");
        }
        if (cm1->getObjectPtr()->image_series_index==5000)
        {
          //  std::cout <<"Cas où index = 5000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "Magn_TI1_CS");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "Magn_TI1_CS");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");
        }
        else if (cm1->getObjectPtr()->image_series_index==6000)
        {
          //  std::cout <<"Cas où index = 6000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "Magn_TI2_CS");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "Magn_TI2_CS");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");
        }
        else if (cm1->getObjectPtr()->image_series_index==7000)
        {
          //  std::cout <<"Cas où index = 7000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "MP2RAGE_CS");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "MP2RAGE_CS");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");
        }
        else if (cm1->getObjectPtr()->image_series_index==8000)
        {
          //  std::cout <<"Cas où index = 8000" << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_DATA_ROLE, GADGETRON_IMAGE_T1MAP);//
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "T1_CS");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "T1_CS");
            //cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, GADGETRON_IMAGE_T1MAP);
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");
            //cm3->getObjectPtr()->set(GADGETRON_IMAGE_COLORMAP,  "HotMetal.pal");
            cm3->getObjectPtr()->set(GADGETRON_IMAGE_COLORMAP,  "Perfusion.pal");//
            //cm3->getObjectPtr()->set(GADGETRON_IMAGE_WINDOWCENTER, (long)(2048));
            //cm3->getObjectPtr()->set(GADGETRON_IMAGE_WINDOWWIDTH, (long)(200));
        }
        else
        {
          //  std::cout <<"Cas où index = autre " << std::endl;
            cm3->getObjectPtr()->set(GADGETRON_IMAGECOMMENT, "Undefined");
            cm3->getObjectPtr()->set(GADGETRON_SEQUENCEDESCRIPTION, "Undefined");
            cm3->getObjectPtr()->set(GADGETRON_IMAGEPROCESSINGHISTORY, "GT");
        }

         cm1->getObjectPtr()->image_series_index += series_offset.at(image_type);

        if (this->next()->putq(cm1) == -1) {
            m1->release();
            GDEBUG("Unable to put extracted images on next gadgets queue");
            return GADGET_FAIL;
        }
    }

    m1->release(); //We have copied all the data in this case

    return GADGET_OK;
}


GADGET_FACTORY_DECLARE(ExtractMP2RAGEGadget)

}
