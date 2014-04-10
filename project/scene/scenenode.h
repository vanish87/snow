/**************************************************************************
**
**   SNOW - CS224 BROWN UNIVERSITY
**
**   scenenode.h
**   Author: mliberma
**   Created: 8 Apr 2014
**
**************************************************************************/

#ifndef SCENENODE_H
#define SCENENODE_H

#include <QList>
#include <glm/mat4x4.hpp>

class Renderable;

class SceneNode
{

public:

    SceneNode( SceneNode *parent = NULL );
    virtual ~SceneNode();

    void clearChildren();
    void addChild( SceneNode *child );

    void clearRenderables();
    void addRenderable( Renderable *renderable );

    virtual void render();

private:

    SceneNode* m_parent;
    glm::mat4 m_transform;

    QList<SceneNode*> m_children;
    QList<Renderable*> m_renderables;

};

#endif // SCENENODE_H