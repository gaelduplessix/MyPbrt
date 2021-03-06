//
//  AreaLight.cpp
//  CSE168_Rendering
//
//  Created by Gael Jochaud du Plessix on 4/23/14.
//
//

#include "AreaLight.h"

#include "Shapes/Mesh.h"

AreaLight* AreaLight::CreateFromMesh(MeshBase* meshBase, const Transform& t, bool inverseNormal,
                                     int indexOffset) {
    Mesh* mesh = dynamic_cast<Mesh*>(meshBase);
    
    if (!mesh) {
        return nullptr;
    }
    
    if (mesh->getTrianglesCount() != 2) {
        return nullptr;
    }
    AreaLight* light = new AreaLight();
    std::shared_ptr<Triangle> triangle = mesh->getTriangle(0);
    vec3
        p1 = t(triangle->getVertex((0+indexOffset)%3)->position),
        p2 = t(triangle->getVertex((1+indexOffset)%3)->position),
        p3 = t(triangle->getVertex((2+indexOffset)%3)->position)
    ;
    light->setPoints(p1, p2, p3);
    vec3 normal = normalize(t.applyToNormal(triangle->getVertex(0)->normal));
    if (inverseNormal) {
        normal = -normal;
    }
    light->setNormal(normal);
    return light;
}

AreaLight::AreaLight() : Light(),
_points(), _normal(),
_intensity(1.0f), _color(std::make_shared<UniformVec3Texture>()),
_isDirectional(false) {
}

AreaLight::~AreaLight() {
    
}

void AreaLight::setPoints(const vec3& p1, const vec3& p2, const vec3& p3) {
    _points[0] = p1;
    _points[1] = p2;
    _points[2] = p3;
}

void AreaLight::setNormal(const vec3 &normal) {
    _normal = normal;
}

void AreaLight::setIntensity(float intensity) {
    _intensity = intensity * M_PI;
}

void AreaLight::setColor(const vec3 &color) {
    _color = std::make_shared<UniformVec3Texture>(color);
}

void AreaLight::setColor(const std::shared_ptr<Texture>& texture) {
    _color = texture;
}

void AreaLight::setDirectional(bool directional) {
    _isDirectional = directional;
}

Spectrum AreaLight::le(const Ray& ray, const Intersection* intersection) const {
    if (_isDirectional) {
        return Spectrum(0.f);
    }
    
    if (ray.tmax == INFINITY) {
        return Spectrum(0.f);
    }
    
    if (dot(-ray.direction, _normal) < 0) {
        return Spectrum(0.f);
    }
    
    vec2 uv;
    if (intersection) {
        uv = intersection->uv;
    }
    
    return Spectrum(_color->evaluateVec3(uv) * (_intensity / (float)M_PI));
}

Spectrum AreaLight::sampleL(const vec3 &point, float rayEpsilon,
                            const LightSample& lightSample,
                            vec3 *wi, VisibilityTester *vt) const {
    if (_isDirectional) {
        *wi = -_normal;
        vt->setRay(point, rayEpsilon, *wi);
        return _intensity * _color->evaluateVec3(vec2(0.f));
    }
    
    vec3 v1 = _points[1] - _points[0], v2 = _points[2] - _points[0];
    vec3 sampledPosition = _points[0] + lightSample.u * v1 + lightSample.v * v2;
    
    vec3 dist = sampledPosition - point;
    float distLength = glm::length(dist);
    dist = dist / distLength;
    vec3 color = _color->evaluateVec3(vec2(lightSample.u, lightSample.v));

    *wi = dist;
    
    vt->setSegment(point, rayEpsilon, sampledPosition);
    
    float area = length(v1) * length(v2);

    return Spectrum(
            (color * (_intensity * area))
            * dot(-dist, _normal)
            * (1.0f / (distLength*distLength)));
}

Spectrum AreaLight::samplePhoton(vec3 *p, vec3 *direction) const {
    float u = (float)rand()/RAND_MAX;
    float v = (float)rand()/RAND_MAX;
    
    vec3 v1 = _points[1] - _points[0], v2 = _points[2] - _points[0];
    vec3 sampledPosition = _points[0] + u * v1 + v * v2;
    
    *p = sampledPosition;
    
    vec3 color = _color->evaluateVec3(vec2(u, v));
    
    float area = length(v1) * length(v2);
    
    if (_isDirectional) {
        *direction = _normal;
        return color * _intensity * area;
    } else {
        // Cosine sample hemisphere direction
        float s = (float)rand()/RAND_MAX;
        float t = (float)rand()/RAND_MAX;
        u = 2.0f*M_PI*s;
        v = sqrt(1.f - t);
        vec3 dir = vec3(v*cos(u), sqrt(t), v*sin(u));
        
        *direction = normalize(normalize(v1)*dir.x + normalize(v2)*dir.z + _normal*dir.y);
    }
    
    return color * _intensity * area * (float)M_PI;
}