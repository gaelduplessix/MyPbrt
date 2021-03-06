//
//  BVHAccelerator.cpp
//  CSE168_Rendering
//
//  Created by Gael Jochaud du Plessix on 4/15/14.
//
//

#include "BVHAccelerator.h"

BVHAccelerator::Node::Node()
: boundingBox(), children(), splitDimension(0), primitivesOffset(0), primitivesCount(0) {
    children[0] = children[1] = nullptr;
}

BVHAccelerator::Node::~Node() {
    if (children[0]) {
        delete children[0];
    }
    if (children[1]) {
        delete children[1];
    }
}

BVHAccelerator::BuildPrimitiveInfo::BuildPrimitiveInfo()
: primitiveIndex(0), centroid(), boundingBox() {
    
}

BVHAccelerator::BuildPrimitiveInfo::BuildPrimitiveInfo(int p, const AABB& bb)
: primitiveIndex(p), centroid(), boundingBox(bb) {
    centroid = 0.5f * bb.min + 0.5f * bb.max;
}

BVHAccelerator::BuildPrimitiveInfo::~BuildPrimitiveInfo() {
    
}

BVHAccelerator::CompareToPoint::CompareToPoint(uint32_t dimension, float point)
: dimension(dimension), point(point) {
    
}

bool BVHAccelerator::CompareToPoint::operator()(const BuildPrimitiveInfo& p) {
    return p.centroid[dimension] < point;
}

BVHAccelerator::ComparePoints::ComparePoints(uint32_t dimension) : dimension(dimension) {
    
}

bool BVHAccelerator::ComparePoints::operator()(const BuildPrimitiveInfo& a,
                                               const BuildPrimitiveInfo& b) {
    return a.centroid[dimension] < b.centroid[dimension];
}

BVHAccelerator::BVHAccelerator(SplitMethod splitMethod)
: _splitMethod(splitMethod), _primitives(), _root(nullptr) {
    
}

BVHAccelerator::~BVHAccelerator() {
    if (_root) {
        delete _root;
    }
}

void BVHAccelerator::preprocess() {
    // First refine the primitives
    std::vector<std::shared_ptr<Primitive>> refined;
    for (const std::shared_ptr<Primitive>& p : _primitives) {
        p->fullyRefine(refined);
    }

    _primitives.swap(refined);
    
    if (!_primitives.size()) {
        _root = nullptr;
        return;
    }
    
    // Initialize data used for building bvh
    std::vector<BuildPrimitiveInfo> buildData;
    buildData.reserve(_primitives.size());
    for (uint32_t i = 0; i < _primitives.size(); ++i) {
        buildData.push_back(BuildPrimitiveInfo(i, _primitives[i]->getBoundingBox()));
    }
    
    // Recursively build BVH tree for primitives
    std::vector<std::shared_ptr<Primitive>> orderedPrimitives;
    orderedPrimitives.reserve(_primitives.size());
    _root = recursiveBuild(buildData, 0, _primitives.size(), orderedPrimitives);
    _primitives.swap(orderedPrimitives);
}

void BVHAccelerator::rebuild() {
    if (_root) {
        delete _root;
    }
    preprocess();
}

/*
 * Structure used by SAH split
 */
struct CompareToBucket {
    
    CompareToBucket(int split, int num, int d, const AABB &b) : centroidBounds(b) {
        splitBucket = split;
        nBuckets = num;
        dim = d;
    }
    
    bool operator()(const BVHAccelerator::BuildPrimitiveInfo &p) const;
    
    int splitBucket, nBuckets, dim;
    const AABB &centroidBounds;
};

bool CompareToBucket::operator()(const BVHAccelerator::BuildPrimitiveInfo &p) const {
    int b = nBuckets * ((p.centroid[dim] - centroidBounds.min[dim]) /
                        (centroidBounds.max[dim] - centroidBounds.min[dim]));
    if (b == nBuckets) b = nBuckets-1;
    return b <= splitBucket;
}

BVHAccelerator::Node* BVHAccelerator::recursiveBuild(std::vector<BuildPrimitiveInfo>& buildData,
                                                     uint32_t start, uint32_t end,
                                                     std::vector<std::shared_ptr<Primitive>>& orderedPrimitives) {
    Node* node = new Node();
    
    // Compute bounding box of all primitives in current node
    AABB bbox;
    for (uint32_t i = start; i < end; ++i) {
        bbox = AABB::Union(bbox, buildData[i].boundingBox);
    }
    
    uint32_t primitivesCount = end - start;
    
    if (primitivesCount < 10) {
        // Create leaf node
        node->primitivesOffset = orderedPrimitives.size();
        node->primitivesCount = primitivesCount;
        for (uint32_t i = start; i < end; ++i) {
            orderedPrimitives.push_back(_primitives[buildData[i].primitiveIndex]);
        }
        node->boundingBox = bbox;
        return node;
    } else {
        // Compute bounding box of primitive centroids used to choose split dimension
        AABB centroidsBB;
        
        for (uint32_t i = start; i < end; ++i) {
            centroidsBB = AABB::Union(centroidsBB, buildData[i].centroid);
        }
        uint32_t splitDimension = centroidsBB.getMaxDimension();
        
        // If bounding box extent is null, create a leaf node
        if (centroidsBB.min[splitDimension] == centroidsBB.max[splitDimension]) {
            node->primitivesOffset = orderedPrimitives.size();
            node->primitivesCount = primitivesCount;
            node->boundingBox = bbox;
            for (uint32_t i = start; i < end; ++i) {
                orderedPrimitives.push_back(_primitives[buildData[i].primitiveIndex]);
            }
            return node;
        }
        
        // Partitions primitives base on splitMethod
        uint32_t mid = (start + end)/2;
        switch (_splitMethod) {
            case SplitMiddle: {
                // Partition primitives through node's bbox middle
                float middle = 0.5f * (centroidsBB.min[splitDimension]
                                       + centroidsBB.max[splitDimension]);
                BuildPrimitiveInfo* midPtr = std::partition(&buildData[start],
                                                            &buildData[end],
                                                            CompareToPoint(splitDimension, middle));
                mid = midPtr - &buildData[0];
                if (mid != start && mid != end) {
                    break;
                }
            }
            case SplitEqualCounts: {
                mid = (start + end)/2;
                std::nth_element(&buildData[start],
                                 &buildData[mid],
                                 &buildData[end],
                                 ComparePoints(splitDimension));
                break;
            }
            case SplitSAH: default: {
                // Partition primitives using approximate SAH
                if (primitivesCount <= 4) {
                    // Partition primitives into equally-sized subsets
                    mid = (start + end) / 2;
                    std::nth_element(&buildData[start], &buildData[mid],
                                     &buildData[end-1]+1, ComparePoints(splitDimension));
                }
                else {
                    const int nBuckets = 12;
                    
                    int bestDimension = -1;
                    int bestSplit = 0;
                    float bestSplitCost = 0.f;
                    
                    // Test all possible split axis
                    for (int testDim = 0; testDim < 3; ++testDim) {
                        if (centroidsBB.max[testDim] == centroidsBB.min[testDim]) {
                            continue;
                        }
                        
                        // Allocate _BucketInfo_ for SAH partition buckets
                        struct BucketInfo {
                            BucketInfo() { count = 0; }
                            int count;
                            AABB bounds;
                        };
                        BucketInfo buckets[nBuckets];
                        
                        // Initialize _BucketInfo_ for SAH partition buckets
                        for (uint32_t i = start; i < end; ++i) {
                            int b = nBuckets *
                            ((buildData[i].centroid[testDim] - centroidsBB.min[testDim]) /
                             (centroidsBB.max[testDim] - centroidsBB.min[testDim]));
                            if (b == nBuckets) b = nBuckets-1;
                            buckets[b].count++;
                            buckets[b].bounds = AABB::Union(buckets[b].bounds, buildData[i].boundingBox);
                        }
                        
                        // Compute costs for splitting after each bucket
                        float cost[nBuckets-1];
                        for (int i = 0; i < nBuckets-1; ++i) {
                            AABB b0, b1;
                            int count0 = 0, count1 = 0;
                            for (int j = 0; j <= i; ++j) {
                                b0 = AABB::Union(b0, buckets[j].bounds);
                                count0 += buckets[j].count;
                            }
                            for (int j = i+1; j < nBuckets; ++j) {
                                b1 = AABB::Union(b1, buckets[j].bounds);
                                count1 += buckets[j].count;
                            }
                            cost[i] = .125f + ((count0*b0.surfaceArea() + count1*b1.surfaceArea())
                                               / bbox.surfaceArea());
                        }
                        
                        // Find bucket to split at that minimizes SAH metric
                        float minCost = cost[0];
                        uint32_t minCostSplit = 0;
                        for (int i = 1; i < nBuckets-1; ++i) {
                            if (cost[i] < minCost) {
                                minCost = cost[i];
                                minCostSplit = i;
                            }
                        }
                        if (bestDimension == -1 || minCost < bestSplitCost) {
                            bestSplitCost = minCost;
                            bestDimension = testDim;
                            bestSplit = minCostSplit;
                        }
                    }
                    
                    // Either create leaf or split primitives at selected SAH bucket
                    if (primitivesCount > 10 || bestSplitCost < primitivesCount) {
                        BuildPrimitiveInfo *pmid = std::partition(&buildData[start],
                                                                  &buildData[end-1]+1,
                                                                  CompareToBucket(bestSplit, nBuckets,
                                                                                  bestDimension, centroidsBB));
                        mid = pmid - &buildData[0];
                        splitDimension = bestDimension;
                    }
                    else {
                        // Create leaf _BVHBuildNode_
                        uint32_t firstPrimOffset = orderedPrimitives.size();
                        for (uint32_t i = start; i < end; ++i) {
                            uint32_t primNum = buildData[i].primitiveIndex;
                            orderedPrimitives.push_back(_primitives[primNum]);
                        }
                        node->primitivesOffset = firstPrimOffset;
                        node->primitivesCount = primitivesCount;
                        node->boundingBox = bbox;
                        return node;
                    }
                }
                break;
            }
        }
        // Init interior node
        node->splitDimension = splitDimension;
        node->children[0] = recursiveBuild(buildData, start, mid, orderedPrimitives);
        node->children[1] = recursiveBuild(buildData, mid, end, orderedPrimitives);
        node->primitivesCount = 0;
        node->boundingBox = bbox;
    }
    return node;
}

void BVHAccelerator::addPrimitive(const std::shared_ptr<Primitive>& primitive) {
    _primitives.push_back(primitive);
}

std::shared_ptr<Primitive> BVHAccelerator::findPrimitive(const std::string& name) {
    for (const std::shared_ptr<Primitive>& p : _primitives) {
        std::shared_ptr<Aggregate> child;
        
        if (p->getName() == name) {
            return p;
        } else if ((child = std::dynamic_pointer_cast<Aggregate>(p))) {
            std::shared_ptr<Primitive> res = child->findPrimitive(name);
            if (res) {
                return res;
            }
        }
    }
    return nullptr;
}

void BVHAccelerator::removePrimitive(const std::string& name) {
    std::remove_if(_primitives.begin(), _primitives.end(), [name] (std::shared_ptr<Primitive> p) {
        return p->getName() == name;
    });
}

const std::vector<std::shared_ptr<Primitive>> BVHAccelerator::getPrimitives() const {
    return _primitives;
}

AABB BVHAccelerator::getBoundingBox() const {
    return _root ? _root->boundingBox : AABB();
}

bool BVHAccelerator::intersect(const Ray& ray, Intersection* intersection) const {
    if (!_root) {
        return false;
    }
    
    bool hit = false;
    
    // Follow ray through BVH nodes to find primitives intersections
    uint32_t todoOffset = 0;
    Node* todo[64];
    Node* currentNode = _root;
    
    float t0, t1;
    
    int nbIntersects = 0;
    while (true) {
        // Check ray against current node
        nbIntersects++;
        if (currentNode->boundingBox.intersectP(ray, &t0, &t1)) {
            if (currentNode->primitivesCount > 0) {
                // Leaf node, check ray against primitives
                for (uint32_t i = 0; i < currentNode->primitivesCount; ++i) {
                    if (_primitives[currentNode->primitivesOffset+i]->intersect(ray, intersection)) {
                        hit = true;
                    }
                }
                if (todoOffset == 0) {
                    // Stack is empty, no more node to check
                    break;
                }
                currentNode = todo[--todoOffset];
            } else {
                // Put far BVH node onto stack, advance to near node
                if (ray.direction[currentNode->splitDimension] < 0) {
                    todo[todoOffset++] = currentNode->children[0];
                    currentNode = currentNode->children[1];
                } else {
                    todo[todoOffset++] = currentNode->children[1];
                    currentNode = currentNode->children[0];
                }
            }
        } else {
            if (todoOffset == 0) {
                // Stack is empty, no more node to check
                break;
            }
            currentNode = todo[--todoOffset];
        }
    }
    
    return hit;
}

bool BVHAccelerator::intersectP(const Ray& ray) const {
    if (!_root) {
        return false;
    }
    
    // Follow ray through BVH nodes to find primitives intersections
    uint32_t todoOffset = 0;
    Node* todo[64];
    Node* currentNode = _root;
    
    float t0, t1;
    
    while (true) {
        // Check ray against current node
        if (currentNode->boundingBox.intersectP(ray, &t0, &t1)) {
            if (currentNode->primitivesCount > 0) {
                // Leaf node, check ray against primitives
                for (uint32_t i = 0; i < currentNode->primitivesCount; ++i) {
                    if (_primitives[currentNode->primitivesOffset+i]->intersectP(ray)) {
                        return true;
                    }
                }
                if (todoOffset == 0) {
                    // Stack is empty, no more node to check
                    break;
                }
                currentNode = todo[--todoOffset];
            } else {
                // Put far BVH node onto stack, advance to near node
                if (ray.direction[currentNode->splitDimension] < 0) {
                    todo[todoOffset++] = currentNode->children[0];
                    currentNode = currentNode->children[1];
                } else {
                    todo[todoOffset++] = currentNode->children[1];
                    currentNode = currentNode->children[0];
                }
            }
        } else {
            if (todoOffset == 0) {
                // Stack is empty, no more node to check
                break;
            }
            currentNode = todo[--todoOffset];
        }
    }
    
    return false;
}