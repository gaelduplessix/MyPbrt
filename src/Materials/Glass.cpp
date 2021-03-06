//
//  Glass.cpp
//  CSE168_Rendering
//
//  Created by Gael Jochaud du Plessix on 4/24/14.
//
//

#include "Glass.h"

#include "Core/Texture.h"

std::shared_ptr<Glass> Glass::Load(const rapidjson::Value& value) {
    std::shared_ptr<Glass> material = std::make_shared<Glass>();
    
    if (value.HasMember("name")) {
        material->setName(value["name"].GetString());
    }
    if (value.HasMember("indexIn")) {
        material->setIndexIn(value["indexIn"].GetDouble());
    }
    if (value.HasMember("indexOut")) {
        material->setIndexOut(value["indexOut"].GetDouble());
    }
    if (value.HasMember("absorptionColor")) {
        std::shared_ptr<Texture> texture = Texture::Load(value["absorptionColor"]);
        if (texture) {
            material->setAbsorptionColor(texture->evaluateVec3(vec2()));
        }
    }
    if (value.HasMember("absorptionCoeff")) {
        material->setAbsorptionCoeff(value["absorptionCoeff"].GetDouble());
    }
    if (value.HasMember("roughness")) {
        material->setRoughness(value["roughness"].GetDouble());
    }
    
    return material;
}

Glass::Glass() : Material(),
_indexIn(1.0f), _indexOut(1.0f),
_absorptionColor(vec3(1.0f)), _absorptionCoeff(0.0f), _roughness(0.2f) {
    
}

Glass::~Glass() {
    
}

std::shared_ptr<Material> Glass::clone() const {
    return std::make_shared<Glass>(*this);
}

void Glass::setIndexIn(float index) {
    _indexIn = index;
}

void Glass::setIndexOut(float index) {
    _indexOut = index;
}

void Glass::setAbsorptionColor(const vec3& color) {
    _absorptionColor = color;
}

void Glass::setAbsorptionCoeff(float coeff) {
    _absorptionCoeff = coeff;
}

void Glass::setRoughness(float roughness) {
    _roughness = roughness;
}

Spectrum Glass::evaluateBSDF(const vec3& wo, const vec3 &wi,
                             const Intersection& intersection) const {
    vec3 h = normalize(wo + wi);
    vec3 n = intersection.normal;
    if (dot(n, wo) < 0) {
        n = -n;
    }
    float blinn = pow(dot(n, h), _roughness);
    if (glm::isnan(blinn)) {
        blinn = 0.f;
    }
    return Spectrum(0.6f) * blinn;
}

Spectrum Glass::sampleBSDF(const vec3 &wo, vec3 *wi, const Intersection &intersection,
                           Material::BxDFType type, BxDFType* sampledType) const {
    const vec3& n = intersection.normal;
    float cosi = glm::abs(glm::dot(wo, n));
    vec3 t;
    float fr = refracted(cosi, wo, intersection.normal, _indexOut, _indexIn, &t);
    
    if (type == BSDFAll) {
        float u = (float)rand()/RAND_MAX;
        if (u > fr) {
            *sampledType = BSDFTransmission;
            *wi = t;
            // If going out of the object, add absorption
            if (glm::dot(wo, n) < 0.0f) {
                return Spectrum(1.0f) * transmittedLight(intersection.t);
            }
            return Spectrum(1.0f);
        } else {
            *sampledType = BSDFReflection;
            *wi = glm::reflect(-wo, n);
            return Spectrum(1.0f);
        }
    }
    else if (type & BSDFReflection) {
        *sampledType = BSDFReflection;
        *wi = glm::reflect(-wo, n);
        return Spectrum(fr);
    } else if (type & BSDFTransmission) {
        *sampledType = BSDFTransmission;
        *wi = t;
        return Spectrum(1.0f - fr);
    } else {
        *sampledType = (BxDFType)0;
        return Spectrum(0.0f);
    }
}

Material::BxDFType Glass::getBSDFType() const {
    return (BxDFType)(BSDFReflection | BSDFTransmission);
}

Spectrum Glass::transmittedLight(float distance) const {
    vec3 alpha = (vec3(1.0) - _absorptionColor)*_absorptionCoeff;
    return Spectrum(glm::exp(-1.0f * alpha * distance));
}