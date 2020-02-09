#ifndef GALOIS_CONNECTIVITYMANAGER_H
#define GALOIS_CONNECTIVITYMANAGER_H


#include <galois/optional.h>
#include "../model/Graph.h"

class ConnectivityManager {
private:
    Graph &graph;
public:
    ConnectivityManager(Graph &graph) : graph(graph) {}

    std::vector<GNode> getNeighbours(GNode node) const {
        std::vector<GNode> vertices;
        for (Graph::edge_iterator ii = graph.edge_begin(node), ee = graph.edge_end(node); ii != ee; ++ii) {
            vertices.push_back(graph.getEdgeDst(ii));
        }
        return vertices;
    }

    std::vector<optional<EdgeIterator>> getTriangleEdges(std::vector<GNode> vertices) {
        std::vector<optional<EdgeIterator>> edges;
        for (int i = 0; i < 3; i++) {
            edges.emplace_back(getEdge(vertices[i], vertices[(i + 1) % 3]));
        }
        return edges;
    }

    optional<EdgeIterator> getEdge(const GNode &v1, const GNode &v2) const {
        EdgeIterator edge = graph.findEdge(v1, v2);
        galois::optional<EdgeIterator> maybeEdge = getOptionalEdge(edge);
        return maybeEdge;
    }

    optional<EdgeIterator> getOptionalEdge(const EdgeIterator &edge) const {
        if (edge.base() == edge.end()) {
            return galois::optional<EdgeIterator>();
        } else {
            return galois::optional<EdgeIterator>(edge);
        }
    }

    bool hasBrokenEdge(const std::vector<optional<EdgeIterator>> &edges) const {
        return countBrokenEdges(edges) > 0;
    }

    int countBrokenEdges(const std::vector<optional<EdgeIterator>> &edges) const {
        int counter = 0;
        for (const optional<EdgeIterator> &edge : edges) {
            if (!edge) {
                counter++;
            }
        }
        return counter;
    }

    std::pair<GNode, EdgeIterator> findSrc(EdgeData edge) const { //TODO: Optimization required
        std::pair<GNode, EdgeIterator> result;
        for (auto n : graph) {
            for (const auto &e : graph.edges(n)) {
                if (graph.getEdgeData(e).getMiddlePoint() == edge.getMiddlePoint()) {
                    result.first = n;
                    result.second = e;
                }
            }
        }
        return result;
    }

    optional<GNode> findNodeBetween(const GNode &node1, const GNode &node2) const { //TODO: Consider optimization
        Coordinates lookThere = (node1->getData().getCoords() + node2->getData().getCoords()) / 2.;
        std::vector<GNode> neighbours1 = getNeighbours(node1);
        std::vector<GNode> neighbours2 = getNeighbours(node2);
        for (GNode &iNode : neighbours1) {
            auto iNodeData = graph.getData(iNode);
            for (GNode &jNode : neighbours2) {
                if (iNodeData.getCoords() == graph.getData(jNode).getCoords() && iNodeData.getCoords() == lookThere) {
                    return optional<GNode>(iNode);
                }
            }
        }
        return optional<GNode>();
    }

    GNode createNode(NodeData &nodeData, galois::UserContext<GNode> &ctx) const {
        GNode node = createNode(nodeData);
        ctx.push(node);
        return std::move(node);
    }

    GNode createNode(NodeData nodeData) const {
        auto node = graph.createNode(nodeData);
        graph.addNode(node);
        return node;
    }

    void createEdge(GNode &node1, GNode &node2, bool border, const Coordinates &middlePoint, double length) {
        graph.addEdge(node1, node2);
        graph.getEdgeData(graph.findEdge(node1, node2)).setBorder(border);
        graph.getEdgeData(graph.findEdge(node1, node2)).setMiddlePoint(middlePoint);
        graph.getEdgeData(graph.findEdge(node1, node2)).setLength(length);
    }

    void createInterior(const GNode &node1, const GNode &node2, const GNode &node3,
                        galois::UserContext<GNode> &ctx) const {
        NodeData interiorData = NodeData{true, false};
        auto interior = createNode(interiorData, ctx);

        graph.addEdge(interior, node1);
        graph.addEdge(interior, node2);
        graph.addEdge(interior, node3);
        interior->getData().setCoords(
                (node1->getData().getCoords() + node2->getData().getCoords() + node3->getData().getCoords()) / 3.);
    }

    GNode createInterior(const GNode &node1, const GNode &node2, const GNode &node3) const {
        NodeData interiorData = NodeData{true, false};
        auto interior = createNode(interiorData);

        graph.addEdge(interior, node1);
        graph.addEdge(interior, node2);
        graph.addEdge(interior, node3);
        interior->getData().setCoords(
                (node1->getData().getCoords() + node2->getData().getCoords() + node3->getData().getCoords()) / 3.);
        return std::move(interior);
    }

    Graph &getGraph() const {
        return graph;
    }

    const std::vector<Coordinates> getVerticesCoords(const GNode &node) const {
        std::vector<Coordinates> result;
        for (auto neighbour: getNeighbours(node)) {
            result.push_back(neighbour->getData().getCoords());
        }
        return result;
    }
};


#endif //GALOIS_CONNECTIVITYMANAGER_H
