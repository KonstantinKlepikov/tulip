/**
 *
 * This file is part of Tulip (http://tulip.labri.fr)
 *
 * Authors: David Auber and the Tulip development Team
 * from LaBRI, University of Bordeaux
 *
 * Tulip is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3
 * of the License, or (at your option) any later version.
 *
 * Tulip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 */
#include <tulip/GraphUpdatesRecorder.h>
#include <tulip/Graph.h>
#include <tulip/GraphImpl.h>

using namespace std;
using namespace tlp;

GraphUpdatesRecorder::GraphUpdatesRecorder(bool allowRestart,
                                           const GraphStorageIdsMemento *prevIdsMemento)
    :
#if !defined(NDEBUG)
      recordingStopped(true),
#endif
      updatesReverted(false), restartAllowed(allowRestart), newValuesRecorded(false),
      oldIdsStateRecorded(prevIdsMemento == nullptr), oldIdsState(prevIdsMemento),
      newIdsState(nullptr) {
}

GraphUpdatesRecorder::~GraphUpdatesRecorder() {
  deleteDeletedObjects();
  deleteValues(oldValues);
  deleteValues(newValues);
  deleteDefaultValues(oldNodeDefaultValues);
  deleteDefaultValues(newNodeDefaultValues);
  deleteDefaultValues(oldEdgeDefaultValues);
  deleteDefaultValues(newEdgeDefaultValues);

  if (oldIdsStateRecorded && oldIdsState)
    delete oldIdsState;

  if (newIdsState)
    delete newIdsState;
}

void GraphUpdatesRecorder::treatEvent(const Event &ev) {
  if (typeid(ev) == typeid(GraphEvent)) {
    const GraphEvent *gEvt = static_cast<const GraphEvent *>(&ev);
    Graph *graph = gEvt->getGraph();

    switch (gEvt->getType()) {
    case GraphEvent::TLP_ADD_NODE:
      addNode(graph, gEvt->getNode());
      break;

    case GraphEvent::TLP_DEL_NODE:
      delNode(graph, gEvt->getNode());
      break;

    case GraphEvent::TLP_ADD_EDGE:
      addEdge(graph, gEvt->getEdge());
      break;

    case GraphEvent::TLP_DEL_EDGE:
      delEdge(graph, gEvt->getEdge());
      break;

    case GraphEvent::TLP_REVERSE_EDGE:
      reverseEdge(graph, gEvt->getEdge());
      break;

    case GraphEvent::TLP_BEFORE_SET_ENDS:
      beforeSetEnds(graph, gEvt->getEdge());
      break;

    case GraphEvent::TLP_AFTER_SET_ENDS:
      afterSetEnds(graph, gEvt->getEdge());
      break;

    case GraphEvent::TLP_ADD_NODES: {
      const std::vector<node> &nodes = graph->nodes();

      for (unsigned int i = nodes.size() - gEvt->getNumberOfNodes(); i < nodes.size(); ++i)
        addNode(graph, nodes[i]);

      break;
    }

    case GraphEvent::TLP_ADD_EDGES:
      addEdges(graph, gEvt->getNumberOfEdges());
      break;

    case GraphEvent::TLP_AFTER_ADD_SUBGRAPH:
      addSubGraph(graph, const_cast<Graph *>(gEvt->getSubGraph()));
      break;

    case GraphEvent::TLP_AFTER_DEL_SUBGRAPH:
      delSubGraph(graph, const_cast<Graph *>(gEvt->getSubGraph()));
      break;

    case GraphEvent::TLP_ADD_LOCAL_PROPERTY:
      addLocalProperty(graph, gEvt->getPropertyName());
      break;

    case GraphEvent::TLP_BEFORE_DEL_LOCAL_PROPERTY:
      delLocalProperty(graph, gEvt->getPropertyName());
      break;

    case GraphEvent::TLP_BEFORE_RENAME_LOCAL_PROPERTY:
      propertyRenamed(gEvt->getProperty());
      break;

    case GraphEvent::TLP_BEFORE_SET_ATTRIBUTE:
      beforeSetAttribute(graph, gEvt->getAttributeName());
      break;

    case GraphEvent::TLP_REMOVE_ATTRIBUTE:
      removeAttribute(graph, gEvt->getAttributeName());

    default:
      break;
    }
  } else {
    const PropertyEvent *propEvt = dynamic_cast<const PropertyEvent *>(&ev);

    if (propEvt) {
      PropertyInterface *prop = propEvt->getProperty();

      switch (propEvt->getType()) {
      case PropertyEvent::TLP_BEFORE_SET_NODE_VALUE:
        beforeSetNodeValue(prop, propEvt->getNode());
        break;

      case PropertyEvent::TLP_BEFORE_SET_ALL_NODE_VALUE:
        beforeSetAllNodeValue(prop);
        break;

      case PropertyEvent::TLP_BEFORE_SET_ALL_EDGE_VALUE:
        beforeSetAllEdgeValue(prop);
        break;

      case PropertyEvent::TLP_BEFORE_SET_EDGE_VALUE:
        beforeSetEdgeValue(prop, propEvt->getEdge());
        break;

      default:
        break;
      }
    }
  }
}

// delete the objects collected as to be deleted
void GraphUpdatesRecorder::deleteDeletedObjects() {

  TLP_HASH_MAP<Graph *, set<PropertyInterface *>> &propertiesToDelete =
      updatesReverted ? addedProperties : deletedProperties;

  std::list<std::pair<Graph *, Graph *>> &subGraphsToDelete =
      updatesReverted ? addedSubGraphs : deletedSubGraphs;

  // loop on properties
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator itdp = propertiesToDelete.begin();

  while (itdp != propertiesToDelete.end()) {
    set<PropertyInterface *>::const_iterator itp = itdp->second.begin();
    set<PropertyInterface *>::const_iterator ite = itdp->second.end();

    while (itp != ite) {
      delete (*itp);
      ++itp;
    }

    ++itdp;
  }

  // loop on sub graphs
  std::list<std::pair<Graph *, Graph *>>::const_iterator itds = subGraphsToDelete.begin();

  while (itds != subGraphsToDelete.end()) {
    itds->second->clearSubGraphs();
    delete itds->second;
    ++itds;
  }
}

// clean up all the MutableContainers
void GraphUpdatesRecorder::deleteValues(TLP_HASH_MAP<PropertyInterface *, RecordedValues> &values) {
  TLP_HASH_MAP<PropertyInterface *, RecordedValues>::const_iterator itv = values.begin();

  while (itv != values.end()) {
    delete itv->second.values;

    if (itv->second.recordedNodes)
      delete itv->second.recordedNodes;

    if (itv->second.recordedEdges)
      delete itv->second.recordedEdges;

    ++itv;
  }

  values.clear();
}

// delete all the DataMem referenced by a TLP_HASH_MAP
void GraphUpdatesRecorder::deleteDefaultValues(
    TLP_HASH_MAP<PropertyInterface *, DataMem *> &values) {
  TLP_HASH_MAP<PropertyInterface *, DataMem *>::const_iterator itv = values.begin();

  while (itv != values.end()) {
    delete itv->second;
    ++itv;
  }

  values.clear();
}

void GraphUpdatesRecorder::recordEdgeContainer(MutableContainer<vector<edge> *> &containers,
                                               GraphImpl *g, node n, edge e) {
  if (!containers.get(n)) {
    vector<edge> *adj = new vector<edge>(g->storage.adj(n));
    // if we got a valid edge, this means that we must register
    // the node adjacencies before that edge was added (see addEdge)
    if (e.isValid()) {
      // as the edge is the last added
      // it must be at the last adj position
      auto size = adj->size() - 1;
      assert(e == (*adj)[size]);
      adj->resize(size);
    }
    containers.set(n, adj);
  }
}

void GraphUpdatesRecorder::recordEdgeContainer(MutableContainer<vector<edge> *> &containers,
                                               GraphImpl *g, node n, const vector<edge> &gEdges,
                                               unsigned int nbAdded) {
  if (!containers.get(n)) {
    vector<edge> *adj = new vector<edge>(g->storage.adj(n));
    // we must ensure that the last edges added in gEdges
    // are previously removed from the current node adjacencies,
    // so we look (in reverse order because they must be at the end)
    // for the elts of adj that are in the last edges added and remove them
    unsigned int adjAdded = 0;
    unsigned int lastAdded = gEdges.size();
    for (unsigned int i = adj->size(); i > 0; --i) {
      edge e = (*adj)[i];
      while (nbAdded) {
        --nbAdded;
        if (e == gEdges[--lastAdded]) {
          ++adjAdded;
          break;
        }
      }
      if (nbAdded == 0)
        break;
    }
    assert(adjAdded);
    adj->resize(adj->size() - adjAdded);
    containers.set(n, adj);
  }
}

void GraphUpdatesRecorder::removeFromEdgeContainer(MutableContainer<vector<edge> *> &containers,
                                                   edge e, node n) {
  vector<edge> *adj = containers.get(n);

  if (adj) {
    vector<edge>::iterator it = adj->begin();

    while (it != adj->end()) {
      if ((*it) == e) {
        adj->erase(it);
        break;
      }

      ++it;
    }
  }
}

void GraphUpdatesRecorder::recordNewValues(GraphImpl *g) {
  assert(restartAllowed);

  if (!newValuesRecorded) {
    // from now on it will be done
    newValuesRecorded = true;

    // get ids memento
    GraphImpl *root = static_cast<GraphImpl *>(g);
    assert(newIdsState == nullptr);

    // record ids memento only if needed
    if (graphAddedNodes.get(g->getId()) || graphAddedEdges.get(g->getId()))
      newIdsState = root->storage.getIdsMemento();

    // record new edges containers
    IteratorValue *itae = addedEdgesEnds.findAllValues(nullptr, false);

    while (itae->hasNext()) {
      TypedValueContainer<std::pair<node, node> *> ends;
      edge e(itae->nextValue(ends));

      // e may have been deleted (see delEdge)
      if (root->isElement(e)) {
        recordEdgeContainer(newContainers, root, ends.value->first);
        recordEdgeContainer(newContainers, root, ends.value->second);
      }
    }

    delete itae;

    // record new properties default values & new values
    // loop on oldNodeDefaultValues
    TLP_HASH_MAP<PropertyInterface *, DataMem *>::const_iterator itdv =
        oldNodeDefaultValues.begin();

    while (itdv != oldNodeDefaultValues.end()) {
      PropertyInterface *p = itdv->first;
      newNodeDefaultValues[p] = p->getNodeDefaultDataMemValue();
      recordNewNodeValues(p);
      ++itdv;
    }

    // loop on node oldValues
    TLP_HASH_MAP<PropertyInterface *, RecordedValues>::const_iterator itov = oldValues.begin();

    while (itov != oldValues.end()) {
      PropertyInterface *p = itov->first;

      if (itov->second.recordedNodes &&
          (oldNodeDefaultValues.find(p) == oldNodeDefaultValues.end()))
        recordNewNodeValues(p);

      ++itov;
    }

    // loop on updatedPropsAddedNodes
    TLP_HASH_MAP<PropertyInterface *, std::set<node>>::const_iterator itan =
        updatedPropsAddedNodes.begin();

    while (itan != updatedPropsAddedNodes.end()) {
      PropertyInterface *p = itan->first;
      TLP_HASH_MAP<PropertyInterface *, RecordedValues>::iterator itnv = newValues.find(p);
      PropertyInterface *nv;
      MutableContainer<bool> *rn;
      bool created = itnv == newValues.end();
      bool hasNewValues = false;

      if (created) {
        nv = p->clonePrototype(p->getGraph(), "");
        rn = new MutableContainer<bool>();
      } else {
        nv = itnv->second.values;
        rn = itnv->second.recordedNodes;

        if (!rn)
          rn = itnv->second.recordedNodes = new MutableContainer<bool>();
      }

      for (auto n : itan->second) {
        if (nv->copy(n, n, p)) {
          rn->set(n, true);
          hasNewValues = true;
        }
      }

      if (created) {
        if (hasNewValues)
          newValues[p] = RecordedValues(nv, rn);
        else {
          delete nv;
          delete rn;
        }
      }

      ++itan;
    }

    // loop on oldEdgeDefaultValues
    itdv = oldEdgeDefaultValues.begin();

    while (itdv != oldEdgeDefaultValues.end()) {
      PropertyInterface *p = itdv->first;
      newEdgeDefaultValues[p] = p->getEdgeDefaultDataMemValue();
      recordNewEdgeValues(p);
      ++itdv;
    }

    // loop on edge oldValues
    itov = oldValues.begin();

    while (itov != oldValues.end()) {
      PropertyInterface *p = itov->first;

      if (itov->second.recordedEdges &&
          (oldEdgeDefaultValues.find(p) == oldEdgeDefaultValues.end()))
        recordNewEdgeValues(p);

      ++itov;
    }

    // loop on updatedPropsAddedEdges
    TLP_HASH_MAP<PropertyInterface *, std::set<edge>>::const_iterator iten =
        updatedPropsAddedEdges.begin();

    while (iten != updatedPropsAddedEdges.end()) {
      PropertyInterface *p = iten->first;
      TLP_HASH_MAP<PropertyInterface *, RecordedValues>::iterator itnv = newValues.find(p);
      PropertyInterface *nv;
      MutableContainer<bool> *re;
      bool created = itnv == newValues.end();
      bool hasNewValues = false;

      if (created) {
        nv = p->clonePrototype(p->getGraph(), "");
        re = new MutableContainer<bool>();
      } else {
        nv = itnv->second.values;
        re = itnv->second.recordedEdges;

        if (!re)
          re = itnv->second.recordedEdges = new MutableContainer<bool>();
      }

      for (auto e : iten->second) {
        if (nv->copy(e, e, p)) {
          re->set(e, true);
          hasNewValues = true;
        }
      }

      if (created) {
        if (hasNewValues)
          newValues[p] = RecordedValues(nv, nullptr, re);
        else {
          delete nv;
          delete re;
        }
      }

      ++iten;
    }

    // record graph attribute new values
    TLP_HASH_MAP<Graph *, DataSet>::const_iterator itav = oldAttributeValues.begin();

    while (itav != oldAttributeValues.end()) {
      Graph *g = itav->first;
      const DataSet &gAttValues = g->getAttributes();
      DataSet &nAttValues = newAttributeValues[g];

      for (const pair<string, DataType *> &pval : itav->second.getValues()) {
        DataType *data = gAttValues.getData(pval.first);
        nAttValues.setData(pval.first, data);
        delete data;
      }

      ++itav;
    }
  }
}

void GraphUpdatesRecorder::recordNewNodeValues(PropertyInterface *p) {
  TLP_HASH_MAP<PropertyInterface *, RecordedValues>::iterator itnv = newValues.find(p);
  assert(itnv == newValues.end() || (itnv->second.recordedNodes == nullptr));

  PropertyInterface *nv;
  MutableContainer<bool> *rn = new MutableContainer<bool>();

  if (itnv == newValues.end())
    nv = p->clonePrototype(p->getGraph(), "");
  else
    nv = itnv->second.values;

  bool hasNewValues = false;

  // record updated nodes new values
  if (oldNodeDefaultValues.find(p) != oldNodeDefaultValues.end()) {
    // loop on non default valuated nodes
    for (auto n : p->getNonDefaultValuatedNodes()) {
      nv->copy(n, n, p);
      rn->set(n, true);
      hasNewValues = true;
    }
  } else {
    TLP_HASH_MAP<PropertyInterface *, RecordedValues>::const_iterator itp = oldValues.find(p);

    if (itp != oldValues.end() && itp->second.recordedNodes) {

      for (unsigned int id : itp->second.recordedNodes->findAll(true)) {
        node n(id);

        if (nv->copy(n, n, p)) {
          rn->set(n, true);
          hasNewValues = true;
        }
      }
    }
  }

  if (hasNewValues) {
    if (itnv == newValues.end())
      newValues[p] = RecordedValues(nv, rn);
    else
      itnv->second.recordedNodes = rn;
  } else {
    delete rn;

    if (itnv == newValues.end())
      delete nv;
  }
}

void GraphUpdatesRecorder::recordNewEdgeValues(PropertyInterface *p) {
  TLP_HASH_MAP<PropertyInterface *, RecordedValues>::iterator itnv = newValues.find(p);
  assert(itnv == newValues.end() || (itnv->second.recordedEdges == nullptr));

  PropertyInterface *nv;
  MutableContainer<bool> *re = new MutableContainer<bool>();

  if (itnv == newValues.end())
    nv = p->clonePrototype(p->getGraph(), "");
  else
    nv = itnv->second.values;

  bool hasNewValues = false;

  // record updated edges new values
  if (oldEdgeDefaultValues.find(p) != oldEdgeDefaultValues.end()) {
    // loop on non default valuated edges
    for (auto e : p->getNonDefaultValuatedEdges()) {
      nv->copy(e, e, p);
      re->set(e, true);
      hasNewValues = true;
    }
  } else {
    TLP_HASH_MAP<PropertyInterface *, RecordedValues>::const_iterator itp = oldValues.find(p);

    if (itp != oldValues.end() && itp->second.recordedEdges) {
      for (unsigned int id : itp->second.recordedEdges->findAll(true)) {
        edge e(id);
        if (nv->copy(e, e, p)) {
          re->set(e, true);
          hasNewValues = true;
        }
      }
    }
  }

  if (hasNewValues) {
    if (itnv == newValues.end())
      newValues[p] = RecordedValues(nv, nullptr, re);
    else
      itnv->second.recordedEdges = re;
  } else {
    delete re;

    if (itnv == newValues.end())
      delete nv;
  }
}

void GraphUpdatesRecorder::startRecording(GraphImpl *g) {
  if (g->getSuperGraph() == g) {
    if (oldIdsState == nullptr)
      oldIdsState = static_cast<GraphImpl *>(g)->storage.getIdsMemento();
  }

  restartRecording(g);
}

void GraphUpdatesRecorder::restartRecording(Graph *g) {
#if !defined(NDEBUG)

  if (g->getSuperGraph() == g) {
    assert(recordingStopped);
    recordingStopped = false;
  }

#endif

  if (newValuesRecorded) {
    deleteValues(newValues);
    deleteValues(newValues);
    deleteDefaultValues(newNodeDefaultValues);
    deleteDefaultValues(newEdgeDefaultValues);

    if (newIdsState)
      delete newIdsState;

    newIdsState = nullptr;
    newValuesRecorded = false;
  }

  g->addListener(this);

  // add self as a PropertyObserver for all previously
  // existing properties
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator itp = addedProperties.find(g);
  const set<PropertyInterface *> *newProps =
      (itp == addedProperties.end()) ? nullptr : &(itp->second);

  for (PropertyInterface *prop : g->getLocalObjectProperties()) {
    if (newProps && (newProps->find(prop) != newProps->end()))
      continue;

    prop->addListener(this);
  }

  // add self as a GraphObserver for all previously
  // existing subgraphs
  const set<Graph *> *newSubGraphs = nullptr;
  set<Graph *> sgSet;
  std::list<std::pair<Graph *, Graph *>>::iterator it = addedSubGraphs.begin();

  for (; it != addedSubGraphs.end(); ++it) {
    if (it->first == g) {
      sgSet.insert(it->second);
    }
  }

  if (!sgSet.empty()) {
    newSubGraphs = &sgSet;
  }

  for (Graph *sg : g->subGraphs()) {
    if (!newSubGraphs || newSubGraphs->find(sg) == newSubGraphs->end())
      restartRecording(sg);
  }
}

void GraphUpdatesRecorder::stopRecording(Graph *g) {
#if !defined(NDEBUG)

  if (g->getSuperGraph() == g) {
    assert(!recordingStopped);
    recordingStopped = true;
  }

#endif
  g->removeListener(this);
  for (PropertyInterface *prop : g->getLocalObjectProperties())
    prop->removeListener(this);
  for (Graph *sg : g->subGraphs())
    stopRecording(sg);
}

void GraphUpdatesRecorder::doUpdates(GraphImpl *g, bool undo) {
  assert(updatesReverted != undo);
  updatesReverted = undo;

  Observable::holdObservers();
  // loop on propsToDel
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>> &propsToDel =
      undo ? addedProperties : deletedProperties;
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator itpg = propsToDel.begin();

  while (itpg != propsToDel.end()) {
    Graph *g = itpg->first;

    for (auto prop : itpg->second) {
      g->delLocalProperty(prop->getName());
    }

    ++itpg;
  }

  // loop on subGraphsToDel
  std::list<std::pair<Graph *, Graph *>> &subGraphsToDel = undo ? addedSubGraphs : deletedSubGraphs;
  std::list<std::pair<Graph *, Graph *>>::const_iterator its = subGraphsToDel.begin();

  while (its != subGraphsToDel.end()) {
    Graph *g = its->first;
    Graph *sg = its->second;

    // remove from list of subgraphs + notify observers
    g->notifyBeforeDelSubGraph(sg);
    g->removeSubGraph(sg);

    if (!undo) {
      // restore its subgraphs as subgraph of its supergraph
      // only if we are redoing its deletion
      for (Graph *ssg : sg->subGraphs()) {
        g->restoreSubGraph(ssg);
      }
    }

    g->notifyAfterDelSubGraph(sg);
    sg->notifyDestroy();

    ++its;
  }

  // loop on edgesToDel
  MutableContainer<GraphEltsRecord *> &edgesToDel = undo ? graphAddedEdges : graphDeletedEdges;
  // edges must be removed in the decreasing order of the graphs ids
  // because for a coherent observation of deleted edges
  // they must be first deleted from a graph before being deleted
  // from the supergraph
  std::set<GraphEltsRecord *> geSet;

  IteratorValue *itge = edgesToDel.findAllValues(nullptr, false);

  while (itge->hasNext()) {
    TypedValueContainer<GraphEltsRecord *> ctnr;
    itge->nextValue(ctnr);
    geSet.insert(ctnr.value);
  }

  delete itge;

  std::set<GraphEltsRecord *>::const_reverse_iterator itrse = geSet.rbegin();

  while (itrse != geSet.rend()) {
    GraphEltsRecord *ger = (*itrse);
    // loop on graph's recorded edges
    for (unsigned int id : ger->elts.findAll(true, true)) {
      edge e(id);

      if (ger->graph->isElement(e))
        ger->graph->removeEdge(e);
    }
    ++itrse;
  }

  // loop on nodesToDel
  MutableContainer<GraphEltsRecord *> &nodesToDel = undo ? graphAddedNodes : graphDeletedNodes;
  IteratorValue *itgn = nodesToDel.findAllValues(nullptr, false);

  while (itgn->hasNext()) {
    TypedValueContainer<GraphEltsRecord *> gnr;
    itgn->nextValue(gnr);

    // loop on graph's recorded nodes
    for (unsigned int id : gnr.value->elts.findAll(true, true)) {
      gnr.value->graph->removeNode(node(id));
    }
  }

  delete itgn;

  // loop on subGraphsToAdd
  std::list<std::pair<Graph *, Graph *>> &subGraphsToAdd = undo ? deletedSubGraphs : addedSubGraphs;
  its = subGraphsToAdd.begin();

  while (its != subGraphsToAdd.end()) {
    Graph *g = its->first;
    Graph *sg = its->second;

    // notify its addition
    g->notifyBeforeAddSubGraph(sg);
    // restore sg as subgraph of g
    g->restoreSubGraph(sg);

    // and sg subgraphs are no longer subgraphs of g
    for (Graph *ssg : sg->subGraphs()) {
      g->removeSubGraph(ssg);
      ssg->setSuperGraph(sg);
    }

    // notify its addition
    g->notifyAfterAddSubGraph(sg);

    ++its;
  }

  // loop on nodesToAdd
  MutableContainer<GraphEltsRecord *> &nodesToAdd = undo ? graphDeletedNodes : graphAddedNodes;
  itgn = nodesToAdd.findAllValues(nullptr, false);

  while (itgn->hasNext()) {
    TypedValueContainer<GraphEltsRecord *> gnr;
    itgn->nextValue(gnr);

    // loop on graph's recorded nodes
    for (unsigned int id : gnr.value->elts.findAll(true, true)) {
      gnr.value->graph->restoreNode(node(id));
    }
  }

  delete itgn;

  // now restore ids manager state
  // this is done before the loop on the edges to add
  // because of some assertion in debug mode
  // while calling the restoreEdge method
  const GraphStorageIdsMemento *idsState = undo ? oldIdsState : newIdsState;

  if (idsState)
    g->storage.restoreIdsMemento(idsState);

  // loop on revertedEdges
  for (auto e : revertedEdges) {
    g->reverse(e);
  }

  // loop on edgesEnds
  TLP_HASH_MAP<edge, pair<node, node>> &updatedEdgesEnds = undo ? oldEdgesEnds : newEdgesEnds;
  TLP_HASH_MAP<edge, pair<node, node>>::const_iterator itee = updatedEdgesEnds.begin();

  while (itee != updatedEdgesEnds.end()) {
    g->setEnds(itee->first, itee->second.first, itee->second.second);
    ++itee;
  }

  // loop on containers
  MutableContainer<std::vector<edge> *> &containers = undo ? oldContainers : newContainers;
  IteratorValue *itc = containers.findAllValues(nullptr, false);

  while (itc->hasNext()) {
    TypedValueContainer<std::vector<edge> *> tvc;
    node n(itc->nextValue(tvc));
    g->storage.restoreAdj(n, *(tvc.value));
  }

  delete itc;

  // loop on edgesToAdd
  MutableContainer<GraphEltsRecord *> &edgesToAdd = undo ? graphDeletedEdges : graphAddedEdges;
  MutableContainer<std::pair<node, node> *> &edgesEnds = undo ? deletedEdgesEnds : addedEdgesEnds;
  // edges must be restored in the increasing order of the graphs ids
  // because for a coherent observation of added edges
  // they must be first added to the supergraph before being added
  // to a graph
  geSet.clear();
  itge = edgesToAdd.findAllValues(nullptr, false);

  while (itge->hasNext()) {
    TypedValueContainer<GraphEltsRecord *> ger;
    itge->nextValue(ger);
    geSet.insert(ger.value);
  }

  delete itge;

  std::set<GraphEltsRecord *>::const_iterator itse = geSet.begin();

  while (itse != geSet.end()) {
    GraphEltsRecord *ger = (*itse);
    // loop on graph's recorded edges
    for (unsigned int id : ger->elts.findAll(true, true)) {
      edge e(id);
      std::pair<node, node> *eEnds = edgesEnds.get(e);

      if (eEnds) {
        ger->graph->restoreEdge(e, eEnds->first, eEnds->second);
      } else {
        // restoration of an edge in a subgraph that was already an element of the root graph
        // (i.e., not a newly added edge)
        ger->graph->restoreEdge(e, ger->graph->getRoot()->source(e),
                                ger->graph->getRoot()->target(e));
      }
    }
    ++itse;
  }

  // loop on propsToAdd
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>> &propsToAdd =
      undo ? deletedProperties : addedProperties;
  itpg = propsToAdd.begin();

  while (itpg != propsToAdd.end()) {
    Graph *g = itpg->first;

    for (auto prop : itpg->second) {
      g->addLocalProperty(prop->getName(), prop);
    }

    ++itpg;
  }

  // loop on renamedProperties
  if (!renamedProperties.empty()) {
    TLP_HASH_MAP<PropertyInterface *, std::string>::iterator itrp = renamedProperties.begin();

    std::vector<std::pair<PropertyInterface *, std::string>> renamings(renamedProperties.size());

    for (unsigned int i = 0; itrp != renamedProperties.end(); ++itrp, ++i) {
      PropertyInterface *prop = itrp->first;
      std::string newName = prop->getName();
      // switch names
      prop->rename(itrp->second);
      renamings[i] = std::make_pair(prop, newName);
    }

    // rebuild
    renamedProperties.clear();

    for (unsigned int i = 0; i < renamings.size(); ++i) {
      const std::pair<PropertyInterface *, std::string> &renaming = renamings[i];
      renamedProperties[renaming.first] = renaming.second;
    }
  }

  // loop on nodeDefaultValues
  TLP_HASH_MAP<PropertyInterface *, DataMem *> &nodeDefaultValues =
      undo ? oldNodeDefaultValues : newNodeDefaultValues;
  TLP_HASH_MAP<PropertyInterface *, DataMem *>::const_iterator itdv = nodeDefaultValues.begin();

  while (itdv != nodeDefaultValues.end()) {
    PropertyInterface *prop = itdv->first;
    prop->setAllNodeDataMemValue(itdv->second);
    ++itdv;
  }

  // loop on edgeDefaultValues
  TLP_HASH_MAP<PropertyInterface *, DataMem *> &edgeDefaultValues =
      undo ? oldEdgeDefaultValues : newEdgeDefaultValues;
  itdv = edgeDefaultValues.begin();

  while (itdv != edgeDefaultValues.end()) {
    PropertyInterface *prop = itdv->first;
    prop->setAllEdgeDataMemValue(itdv->second);
    ++itdv;
  }

  // loop on recorded values
  TLP_HASH_MAP<PropertyInterface *, RecordedValues> &rvalues = undo ? oldValues : newValues;
  TLP_HASH_MAP<PropertyInterface *, RecordedValues>::const_iterator itrv = rvalues.begin();

  while (itrv != rvalues.end()) {
    PropertyInterface *prop = itrv->first;
    PropertyInterface *nv = itrv->second.values;

    if (itrv->second.recordedNodes) {

      for (unsigned int id : itrv->second.recordedNodes->findAllValues(false, false)) {
        node n(id);
        prop->copy(n, n, nv);
      }
    }

    if (itrv->second.recordedEdges) {

      for (unsigned int id : itrv->second.recordedEdges->findAllValues(false, false)) {
        edge e(id);
        prop->copy(e, e, nv);
      }
    }

    ++itrv;
  }

  // loop on attribute values to restore
  TLP_HASH_MAP<Graph *, DataSet> &attValues = undo ? oldAttributeValues : newAttributeValues;
  TLP_HASH_MAP<Graph *, DataSet>::const_iterator itav = attValues.begin();

  while (itav != attValues.end()) {
    Graph *g = itav->first;

    for (const pair<string, DataType *> &pval : itav->second.getValues()) {
      if (pval.second)
        g->getNonConstAttributes().setData(pval.first, pval.second);
      else
        g->getNonConstAttributes().remove(pval.first);
    }

    ++itav;
  }

  Observable::unholdObservers();
}

bool GraphUpdatesRecorder::hasUpdates() {
  assert(updatesReverted == false);

  // check addedProperties
  if (addedProperties.begin() != addedProperties.end())
    return true;

  // check addedSubGraphs
  if (addedSubGraphs.begin() != addedSubGraphs.end())
    return true;

  // check graphAddedEdges
  IteratorValue *itv = graphAddedEdges.findAllValues(nullptr, false);
  bool updated = itv->hasNext();
  delete itv;

  if (updated)
    return true;

  // check graphAddedNodes
  itv = graphAddedNodes.findAllValues(nullptr, false);

  while (itv->hasNext()) {
    TypedValueContainer<GraphEltsRecord *> gnr;
    itv->nextValue(gnr);

    // loop on graph's recorded nodes
    Iterator<unsigned int> *itn = gnr.value->elts.findAll(true, true);
    updated = itn->hasNext();
    delete itn;

    if (updated)
      break;
  }

  delete itv;

  if (updated)
    return true;

  // check deletedSubGraphs
  if (deletedSubGraphs.begin() != deletedSubGraphs.end())
    return true;

  // check graphDeletedNodes
  itv = graphDeletedNodes.findAllValues(nullptr, false);
  updated = itv->hasNext();
  delete itv;

  if (updated)
    return true;

  // check revertedEdges
  if (revertedEdges.begin() != revertedEdges.end())
    return true;

  // check oldEdgesEnds
  if (oldEdgesEnds.begin() != oldEdgesEnds.end())
    return true;

  // check oldcontainers
  itv = oldContainers.findAllValues(nullptr, false);
  updated = itv->hasNext();
  delete itv;

  if (updated)
    return true;

  // check graphDeletedEdges
  itv = graphDeletedEdges.findAllValues(nullptr, false);
  updated = itv->hasNext();
  delete itv;

  if (updated)
    return true;

  // check deletedProperties
  if (deletedProperties.begin() != deletedProperties.end())
    return true;

  // check renamedProperties
  if (!renamedProperties.empty())
    return true;

  // check oldNodeDefaultValues
  if (oldNodeDefaultValues.begin() != oldNodeDefaultValues.end())
    return true;

  // check oldEdgeDefaultValues
  if (oldEdgeDefaultValues.begin() != oldEdgeDefaultValues.end())
    return true;

  // check oldValues
  if (oldValues.begin() != oldValues.end())
    return true;

  // check oldAttributeValues
  if (oldAttributeValues.begin() != oldAttributeValues.end())
    return true;

  return false;
}

bool GraphUpdatesRecorder::dontObserveProperty(PropertyInterface *prop) {
  if (!restartAllowed) {
    // check if nothing is yet recorded for prop
    if ((oldNodeDefaultValues.find(prop) == oldNodeDefaultValues.end()) &&
        (oldEdgeDefaultValues.find(prop) == oldEdgeDefaultValues.end()) &&
        (oldValues.find(prop) == oldValues.end()) &&
        (updatedPropsAddedNodes.find(prop) == updatedPropsAddedNodes.end()) &&
        (updatedPropsAddedEdges.find(prop) == updatedPropsAddedEdges.end())) {
      // prop is no longer observed
      prop->removeListener(this);
      // may be a newly added property
      Graph *g = prop->getGraph();
      TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::iterator it = addedProperties.find(g);

      if (it != addedProperties.end() && (it->second.find(prop) != it->second.end()))
        // the property is no longer recorded
        it->second.erase(prop);

      return true;
    }
  }

  return false;
}

bool GraphUpdatesRecorder::isAddedOrDeletedProperty(Graph *g, PropertyInterface *prop) {
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator it = addedProperties.find(g);

  if (it != addedProperties.end() && (it->second.find(prop) != it->second.end()))
    return true;

  it = deletedProperties.find(g);
  return it != deletedProperties.end() && (it->second.find(prop) != it->second.end());
}

void GraphUpdatesRecorder::addNode(Graph *g, node n) {
  GraphEltsRecord *gnr = graphAddedNodes.get(g->getId());

  if (gnr == nullptr) {
    gnr = new GraphEltsRecord(g);
    graphAddedNodes.set(g->getId(), gnr);
  }

  gnr->elts.set(n, true);

  if (g->getRoot() == g) {
    addedNodes.set(n, true);
  }

  // we need to backup properties values of the newly added node
  // in order to restore them when reading the node through the tlp::Graph::unpop() method
  // as the default properties values might change
  for (PropertyInterface *prop : g->getLocalObjectProperties()) {
    beforeSetNodeValue(prop, n);
  }
}

void GraphUpdatesRecorder::addEdge(Graph *g, edge e) {
  GraphEltsRecord *ger = graphAddedEdges.get(g->getId());

  if (ger == nullptr) {
    ger = new GraphEltsRecord(g);
    graphAddedEdges.set(g->getId(), ger);
  }

  ger->elts.set(e, true);

  if (g == g->getRoot()) {
    auto eEnds = g->ends(e);
    addedEdgesEnds.set(e, new std::pair<node, node>(eEnds));
    // record source & target old adjacencies
    recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.first, e);
    recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.second, e);
  }

  // we need to backup properties values of the newly added edge
  // in order to restore them when reading the node through the tlp::Graph::unpop() method
  // as the default properties values can change
  for (PropertyInterface *prop : g->getLocalObjectProperties()) {
    beforeSetEdgeValue(prop, e);
  }
}

void GraphUpdatesRecorder::addEdges(Graph *g, unsigned int nbAdded) {
  GraphEltsRecord *ger = graphAddedEdges.get(g->getId());

  if (ger == nullptr) {
    ger = new GraphEltsRecord(g);
    graphAddedEdges.set(g->getId(), ger);
  }

  const std::vector<edge> &gEdges = g->edges();

  for (unsigned int i = gEdges.size() - nbAdded; i < gEdges.size(); ++i) {
    edge e = gEdges[i];
    ger->elts.set(e, true);

    if (g == g->getRoot()) {
      auto eEnds = g->ends(e);
      addedEdgesEnds.set(e, new std::pair<node, node>(eEnds));
      // record source & target old adjacencies
      recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.first, gEdges, nbAdded);
      recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.second, gEdges,
                          nbAdded);
    }

    // we need to backup properties values of the newly added edge
    // in order to restore them when reading the node through the tlp::Graph::unpop() method
    // as the default properties values can change
    for (PropertyInterface *prop : g->getLocalObjectProperties()) {
      beforeSetEdgeValue(prop, e);
    }
  }
}

void GraphUpdatesRecorder::delNode(Graph *g, node n) {
  GraphEltsRecord *gnr = graphAddedNodes.get(g->getId());

  if (gnr != nullptr && gnr->elts.get(n)) {
    // remove n from graph's recorded nodes if it is a newly added node
    gnr->elts.set(n, false);
    // but don't remove it from addedNodes
    // to ensure further erasal from property will not
    // record a value as if it was a preexisting node
    return;
  }

  // insert n into graphDeletedNodes
  gnr = graphDeletedNodes.get(g->getId());

  if (gnr == nullptr) {
    gnr = new GraphEltsRecord(g);
    graphDeletedNodes.set(g->getId(), gnr);
  }

  gnr->elts.set(n, true);

  // get the set of added properties if any
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator itp = addedProperties.find(g);
  const set<PropertyInterface *> *newProps =
      (itp == addedProperties.end()) ? nullptr : &(itp->second);

  for (PropertyInterface *prop : g->getLocalObjectProperties()) {
    // nothing to record for newly added properties
    if (newProps && (newProps->find(prop) != newProps->end()))
      continue;

    beforeSetNodeValue(prop, n);
  }

  if (g == g->getSuperGraph())
    recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), n);
}

void GraphUpdatesRecorder::delEdge(Graph *g, edge e) {
  GraphEltsRecord *ger = graphAddedEdges.get(g->getId());

  // remove e from addedEdges if it is a newly added edge
  if (ger != nullptr && ger->elts.get(e)) {
    ger->elts.set(e, false);
    // do not remove from addedEdgesEnds
    // to ensure further erasal from property will not
    // record a value as if it was a preexisting edge
    /* if (graphs.empty())
    addedEdges.erase(it); */
    // remove from revertedEdges if needed
    set<edge>::iterator itR = revertedEdges.find(e);

    if (itR != revertedEdges.end())
      revertedEdges.erase(itR);

    // remove edge from nodes newContainers if needed
    std::pair<node, node> *eEnds = addedEdgesEnds.get(e);

    if (eEnds) {
      removeFromEdgeContainer(newContainers, e, eEnds->first);
      removeFromEdgeContainer(newContainers, e, eEnds->second);
    }

    return;
  }

  // insert e into graph's deleted edges
  ger = graphDeletedEdges.get(g->getId());

  if (ger == nullptr) {
    ger = new GraphEltsRecord(g);
    graphDeletedEdges.set(g->getId(), ger);
  }

  if (deletedEdgesEnds.get(e) == nullptr) {
    auto eEnds = g->ends(e);

    if (g == g->getSuperGraph()) {
      // remove from revertedEdges if needed
      set<edge>::iterator it = revertedEdges.find(e);

      if (it != revertedEdges.end()) {
        revertedEdges.erase(it);
        deletedEdgesEnds.set(e, new std::pair<node, node>(eEnds.second, eEnds.first));
      } else {

        TLP_HASH_MAP<edge, pair<node, node>>::const_iterator ite = oldEdgesEnds.find(e);

        if (ite == oldEdgesEnds.end())
          deletedEdgesEnds.set(e, new std::pair<node, node>(eEnds));
        else {
          deletedEdgesEnds.set(e, new std::pair<node, node>(ite->second));
          // remove from oldEdgesEnds
          oldEdgesEnds.erase(ite);
          // remove from newEdgesEnds
          newEdgesEnds.erase(e);
        }
      }
    } else
      deletedEdgesEnds.set(e, new std::pair<node, node>(eEnds));
  }

  ger->elts.set(e, true);

  // get the set of added properties if any
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator itp = addedProperties.find(g);
  const set<PropertyInterface *> *newProps =
      (itp == addedProperties.end()) ? nullptr : &(itp->second);

  // loop on properties to save the edge's associated values
  for (PropertyInterface *prop : g->getLocalObjectProperties()) {
    // nothing to record for newly added properties
    if (newProps && (newProps->find(prop) != newProps->end()))
      continue;

    beforeSetEdgeValue(prop, e);
  }

  if (g == g->getRoot()) {
    // record source & target old containers
    const pair<node, node> &eEnds = g->ends(e);
    recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.first);
    recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.second);
  }
}

void GraphUpdatesRecorder::reverseEdge(Graph *g, edge e) {
  if (g == g->getSuperGraph()) {
    pair<node, node> *eEnds = addedEdgesEnds.get(e);

    // if it is a newly added edge revert its source and target
    if (eEnds != nullptr) {
      node src = eEnds->first;
      eEnds->first = eEnds->second;
      eEnds->second = src;
      return;
    }

    TLP_HASH_MAP<edge, pair<node, node>>::iterator itne = newEdgesEnds.find(e);

    if (itne != newEdgesEnds.end()) {
      // revert ends of itne
      node src = itne->second.first;
      itne->second.first = itne->second.second;
      itne->second.second = src;
    } else { // update reverted edges
      set<edge>::iterator it = revertedEdges.find(e);

      if (it != revertedEdges.end())
        revertedEdges.erase(it);
      else {
        revertedEdges.insert(e);
        // record source & target old containers
        const pair<node, node> &eEnds = g->ends(e);
        recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.first);
        recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), eEnds.second);
      }
    }
  }
}

void GraphUpdatesRecorder::beforeSetEnds(Graph *g, edge e) {
  if (g == g->getSuperGraph() && oldEdgesEnds.find(e) == oldEdgesEnds.end() &&
      addedEdgesEnds.get(e) == nullptr) {
    pair<node, node> ends = g->ends(e);
    set<edge>::iterator it = revertedEdges.find(e);

    // if it is a reverted edge
    // remove it from the set
    if (it != revertedEdges.end()) {
      revertedEdges.erase(it);
      // revert ends of it
      node tgt = ends.first;
      ends.first = ends.second;
      ends.second = tgt;
    } else {
      // record source & target old containers
      recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), ends.first);
      recordEdgeContainer(oldContainers, static_cast<GraphImpl *>(g), ends.second);
    }

    // add e old ends in oldEdgesEnds
    oldEdgesEnds[e] = ends;
  }
}

void GraphUpdatesRecorder::afterSetEnds(Graph *g, edge e) {
  if (g == g->getSuperGraph()) {
    const pair<node, node> &ends = g->ends(e);
    std::pair<node, node> *eEnds = addedEdgesEnds.get(e);

    // if it is a newly added edge update its source and target
    if (eEnds != nullptr) {
      eEnds->first = ends.first;
      eEnds->second = ends.second;
      return;
    }

    // update new ends in newEdgesEnds
    newEdgesEnds[e] = ends;
  }
}

void GraphUpdatesRecorder::addSubGraph(Graph *g, Graph *sg) {
  // last added subgraph will be deleted first during undo/redo
  addedSubGraphs.push_front(std::make_pair(g, sg));

  // sg may already have nodes and edges
  // cf addCloneSubGraph
  if (sg->numberOfNodes()) {

    for (auto n : sg->nodes()) {
      addNode(sg, n);
    }

    for (auto e : sg->edges()) {
      addEdge(sg, e);
    }
  }

  sg->addListener(this);
}

void GraphUpdatesRecorder::delSubGraph(Graph *g, Graph *sg) {

  std::pair<Graph *, Graph *> p = std::make_pair(g, sg);

  std::list<std::pair<Graph *, Graph *>>::iterator it =
      std::find(addedSubGraphs.begin(), addedSubGraphs.end(), p);

  // remove sg from addedSubGraphs if it is a newly added subgraph
  if (it != addedSubGraphs.end()) {

    addedSubGraphs.erase(it);

    // remove any update data concerning the removed subgraph
    // as it will be deleted
    removeGraphData(sg);

    // but set its subgraphs as added in its supergraph
    for (Graph *ssg : sg->subGraphs()) {
      addSubGraph(g, ssg);
    }

    return;
  }

  // last deleted subgraph will be the last one created during undo/redo
  deletedSubGraphs.push_back(p);

  // sg is no longer observed
  sg->removeListener(this);

  // but it must not be really deleted
  g->setSubGraphToKeep(sg);
}

void GraphUpdatesRecorder::removeGraphData(Graph *g) {
  for (Graph *sg : g->subGraphs()) {
    std::pair<Graph *, Graph *> p = std::make_pair(g, sg);
    std::list<std::pair<Graph *, Graph *>>::iterator it =
        std::find(addedSubGraphs.begin(), addedSubGraphs.end(), p);

    if (it != addedSubGraphs.end()) {
      addedSubGraphs.erase(it);
    }
  }
  graphAddedNodes.set(g->getId(), nullptr);
  graphDeletedNodes.set(g->getId(), nullptr);
  graphAddedEdges.set(g->getId(), nullptr);
  graphDeletedEdges.set(g->getId(), nullptr);
  addedProperties.erase(g);
  deletedProperties.erase(g);
  oldAttributeValues.erase(g);
  newAttributeValues.erase(g);
}

void GraphUpdatesRecorder::addLocalProperty(Graph *g, const string &name) {
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::const_iterator it = addedProperties.find(g);

  PropertyInterface *prop = g->getProperty(name);

  if (it == addedProperties.end()) {
    set<PropertyInterface *> props;
    props.insert(prop);
    addedProperties[g] = props;
  } else
    addedProperties[g].insert(prop);
}

void GraphUpdatesRecorder::delLocalProperty(Graph *g, const string &name) {
  PropertyInterface *prop = g->getProperty(name);

  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::iterator it = addedProperties.find(g);

  // remove p from addedProperties if it is a newly added one
  if (it != addedProperties.end() && (it->second.find(prop) != it->second.end())) {
    // the property is no longer recorded
    it->second.erase(prop);
    // remove from renamed properties
    // if needed
    TLP_HASH_MAP<PropertyInterface *, std::string>::iterator itr = renamedProperties.find(prop);

    if (itr != renamedProperties.end())
      renamedProperties.erase(itr);

    updatedPropsAddedNodes.erase(prop);
    updatedPropsAddedEdges.erase(prop);

    return;
  }

  // insert p into deletedProperties
  it = deletedProperties.find(g);

  if (it == deletedProperties.end()) {
    set<PropertyInterface *> props;
    props.insert(prop);
    deletedProperties[g] = props;
  } else
    deletedProperties[g].insert(prop);

  // the property is no longer observed
  prop->removeListener(this);
}

void GraphUpdatesRecorder::propertyRenamed(PropertyInterface *prop) {
  TLP_HASH_MAP<Graph *, set<PropertyInterface *>>::iterator it =
      addedProperties.find(prop->getGraph());

  // remove p from addedProperties if it is a newly added one
  if (it != addedProperties.end() && (it->second.find(prop) != it->second.end())) {
    return;
  } else {
    if (renamedProperties.find(prop) == renamedProperties.end())
      renamedProperties[prop] = prop->getName();
  }
}

void GraphUpdatesRecorder::beforeSetNodeValue(PropertyInterface *p, node n) {
  // dont record the old value if the default one has been changed
  if (oldNodeDefaultValues.find(p) != oldNodeDefaultValues.end())
    return;

  // don't record old values for newly added nodes
  if (addedNodes.get(n)) {
    if (!restartAllowed)
      return;
    else {
      if (p->getGraph()->isElement(n))
        updatedPropsAddedNodes[p].insert(n);
      else
        // n has been deleted in the whole graph hierarchy, so we don't
        // need to backup its property value in the next push as the node
        // does not belong to a graph anymore
        updatedPropsAddedNodes[p].erase(n);
    }
  } else {
    TLP_HASH_MAP<PropertyInterface *, RecordedValues>::iterator it = oldValues.find(p);

    if (it == oldValues.end()) {
      PropertyInterface *pv = p->clonePrototype(p->getGraph(), "");
      MutableContainer<bool> *rn = new MutableContainer<bool>();

      pv->copy(n, n, p);
      rn->set(n, true);
      oldValues[p] = RecordedValues(pv, rn);
    }
    // check for a previously recorded old value
    else {
      if (it->second.recordedNodes) {
        if (it->second.recordedNodes->get(n))
          return;
      } else
        it->second.recordedNodes = new MutableContainer<bool>();

      it->second.values->copy(n, n, p);
      it->second.recordedNodes->set(n, true);
    }
  }
}

void GraphUpdatesRecorder::beforeSetAllNodeValue(PropertyInterface *p) {
  if (oldNodeDefaultValues.find(p) == oldNodeDefaultValues.end()) {
    // first save the already existing value for all non default valuated nodes
    for (auto n : p->getNonDefaultValuatedNodes())
      beforeSetNodeValue(p, n);
    // then record the old default value
    // because beforeSetNodeValue does nothing if it has already been changed
    oldNodeDefaultValues[p] = p->getNodeDefaultDataMemValue();
  }
}

void GraphUpdatesRecorder::beforeSetEdgeValue(PropertyInterface *p, edge e) {
  // dont record the old value if the default one has been changed
  if (oldEdgeDefaultValues.find(p) != oldEdgeDefaultValues.end())
    return;

  // dont record old value for newly added edge
  if (addedEdgesEnds.get(e)) {
    if (!restartAllowed)
      return;

    if (p->getGraph()->isElement(e))
      updatedPropsAddedEdges[p].insert(e);
    else {
      // e has been deleted in the whole graph hierarchy, so we don't
      // need to backup its property value in the next push as the edge
      // does not belong to a graph anymore
      updatedPropsAddedEdges[p].erase(e);
    }
  } else {
    TLP_HASH_MAP<PropertyInterface *, RecordedValues>::iterator it = oldValues.find(p);

    if (it == oldValues.end()) {
      PropertyInterface *pv = p->clonePrototype(p->getGraph(), "");
      MutableContainer<bool> *re = new MutableContainer<bool>();

      pv->copy(e, e, p);
      re->set(e, true);
      oldValues[p] = RecordedValues(pv, nullptr, re);
    }
    // check for a previously recorded old value
    else {
      if (it->second.recordedEdges) {
        if (it->second.recordedEdges->get(e))
          return;
      } else
        it->second.recordedEdges = new MutableContainer<bool>();

      it->second.values->copy(e, e, p);
      it->second.recordedEdges->set(e, true);
    }
  }
}

void GraphUpdatesRecorder::beforeSetAllEdgeValue(PropertyInterface *p) {
  if (oldEdgeDefaultValues.find(p) == oldEdgeDefaultValues.end()) {
    // first save the already existing value for all non default valuated edges
    for (auto e : p->getNonDefaultValuatedEdges())
      beforeSetEdgeValue(p, e);
    // then record the old default value
    // because beforeSetEdgeValue does nothing if it has already been changed
    oldEdgeDefaultValues[p] = p->getEdgeDefaultDataMemValue();
  }
}

void GraphUpdatesRecorder::beforeSetAttribute(Graph *g, const std::string &name) {
  TLP_HASH_MAP<Graph *, DataSet>::iterator it = oldAttributeValues.find(g);

  if (it != oldAttributeValues.end() && it->second.exists(name))
    return;

  // save the previously existing value
  DataType *valType = g->getAttributes().getData(name);
  oldAttributeValues[g].setData(name, valType);
  delete valType;
}

void GraphUpdatesRecorder::removeAttribute(Graph *g, const std::string &name) {
  beforeSetAttribute(g, name);
}
