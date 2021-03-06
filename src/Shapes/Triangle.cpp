//
//  Triangle.cpp
//  CSE168_Rendering
//
//  Created by Gael Jochaud du Plessix on 4/4/14.
//
//

#include "Triangle.h"

#include "Core/Ray.h"
#include "Shapes/Mesh.h"
#include "Core/Material.h"

Triangle::Triangle() : _vertices(), _mesh(), _material() {
    
}

Triangle::Triangle(uint_t a, uint_t b, uint_t c) : _vertices(), _mesh(), _material() {
    _vertices[0] = a;
    _vertices[1] = b;
    _vertices[2] = c;    
}

Triangle::~Triangle() {
}

void Triangle::setMesh(const std::shared_ptr<const Mesh>& mesh) {
    _mesh = mesh;
}

void Triangle::setMaterial(const std::shared_ptr<Material>& material) {
    _material = material;
}

void Triangle::setVertices(uint_t a, uint_t b, uint_t c) {
    _vertices[0] = a;
    _vertices[1] = b;
    _vertices[2] = c;
}

const Vertex* Triangle::getVertex(int num) const {
    return _mesh->getVertex(num);
}

bool Triangle::intersect(const Ray& ray, Intersection* intersection) const {
    const Vertex* v0 = _mesh->getVertex(_vertices[0]);
    const Vertex* v1 = _mesh->getVertex(_vertices[1]);
    const Vertex* v2 = _mesh->getVertex(_vertices[2]);
    
    const vec3 &a = v0->position, &b = v1->position, &c = v2->position;
    vec3 oa = ray.origin - a, ca = c - a, ba = b - a;
    vec3 normal = cross(ba, ca);
    float det = dot(-ray.direction, normal);
    
    // Determinant null
    if (det == 0.0f) {
        return false;
    }
    
    float alpha = dot(-ray.direction, cross(oa, ca)) / det;
    if (alpha <= 0 || alpha >= 1) {
        return false;
    }
    
    float beta = dot(-ray.direction, cross(ba, oa)) / det;
    if (beta <= 0 || beta >= 1) {
        return false;
    }
    
    if (alpha + beta >= 1) {
        return false;
    }
    
    float t = dot(oa, normal) / det;
    if (t < ray.tmin || t > ray.tmax) {
        return false;
    }
    
    // Compute uv coords
    vec2 uvs = ((1-alpha-beta)*v0->texCoord
                + alpha*v1->texCoord
                + beta*v2->texCoord);
    
    // Reject if we have an alpha texture
    const Texture* alphaTexture = nullptr;
    if (_mesh && _mesh->_alphaTexture) {
        alphaTexture = _mesh->_alphaTexture.get();
    } else if (_material) {
        alphaTexture = _material->getAlphaTexture();
    }
    if (alphaTexture && alphaTexture->evaluateFloat(uvs) < 0.1f) {
        return false;
    }
    
    // Compute smoothed normal
    normal = ((1-alpha-beta)*v0->normal
              + alpha*v1->normal
              + beta*v2->normal);
    
    // Update the ray
    ray.tmax = t;
    
    // Fill in intersection informations
    intersection->t = t;
    intersection->point = ray(t);
    intersection->normal = normal;
    intersection->uv = uvs;
    
    // Interpolate tangents
    intersection->tangentU = ((1-alpha-beta)*v0->tangentU
                              + alpha*v1->tangentU
                              + beta*v2->tangentU);
    intersection->tangentV = ((1-alpha-beta)*v0->tangentV
                              + alpha*v1->tangentV
                              + beta*v2->tangentV);
    
    if (_material) {
        intersection->material = _material.get();
    }

    intersection->rayEpsilon = 1e-3f * t;
    return true;
}

bool Triangle::intersectP(const Ray& ray) const {
    const Vertex* v0 = _mesh->getVertex(_vertices[0]);
    const Vertex* v1 = _mesh->getVertex(_vertices[1]);
    const Vertex* v2 = _mesh->getVertex(_vertices[2]);
    
    const vec3 &a = v0->position, &b = v1->position, &c = v2->position;
    vec3 oa = ray.origin - a, ca = c - a, ba = b - a;
    vec3 normal = cross(ba, ca);
    float det = dot(-ray.direction, normal);
    
    // Determinant null
    if (det == 0.0f) {
        return false;
    }
    
    float alpha = dot(-ray.direction, cross(oa, ca)) / det;
    if (alpha <= 0 || alpha >= 1) {
        return false;
    }
    
    float beta = dot(-ray.direction, cross(ba, oa)) / det;
    if (beta <= 0 || alpha + beta >= 1) {
        return false;
    }
    
    float t = dot(oa, normal) / det;
    if (t < ray.tmin || t > ray.tmax) {
        return false;
    }
    
    // Compute uv coords
    vec2 uvs = ((1-alpha-beta)*v0->texCoord
                + alpha*v1->texCoord
                + beta*v2->texCoord);
    
    // Reject if we have an alpha texture
    if (_mesh && _mesh->_alphaTexture) {
        if (_mesh->_alphaTexture->evaluateFloat(uvs) == 0.0f) {
            return false;
        }
    }
    
    return true;
}

AABB Triangle::getBoundingBox() const {
    return AABB::Union(AABB(getVertex(_vertices[0])->position, getVertex(_vertices[1])->position),
                       getVertex(_vertices[2])->position);
}

bool Triangle::hasMaterial() const {
    return (bool)_material;
}