/**************************************************************************
**
**   SNOW - CS224 BROWN UNIVERSITY
**
**   mitsubaexporter.cpp
**   Author: evjang
**   Created: 13 Apr 2014
**
**************************************************************************/

#include "mitsubaexporter.h"
#include <QFile>
#include <iostream>
#include "scene/scenenode.h"
#include "geometry/bbox.h"
#include "scene/scene.h"
#include "sim/particle.h"
#include "sim/engine.h"
#include "viewport/camera.h"
#include "sim/collider.h" // implicit colliders
#include "math.h"
#include <fstream>
#include <iomanip>
#include "common/common.h"
#include <stdio.h>

MitsubaExporter::MitsubaExporter()
{
    m_nodes = NULL;
    m_spf = 1.f/24.f;
    m_lastUpdateTime = 0.f;
    m_frame = 0;
}

MitsubaExporter::MitsubaExporter(QString fprefix, float fps)
    : m_fileprefix(fprefix), m_fps(fps)
{
    m_nodes = NULL;
    m_spf = 1.f/m_fps;
    m_lastUpdateTime = 0.f;
    m_frame = 0;
}

MitsubaExporter::~MitsubaExporter()
{
    SAFE_DELETE_ARRAY(m_nodes);
}

float MitsubaExporter::getspf() {return m_spf;}
float MitsubaExporter::getLastUpdateTime() {return m_lastUpdateTime;}

void MitsubaExporter::reset(Grid grid)
{
    SAFE_DELETE_ARRAY(m_nodes);
//    SAFE_DELETE_ARRAY(m_densities);
//    SAFE_DELETE_ARRAY(m_albedo);
    m_grid = grid;
    m_bbox = BBox(grid.pos, grid.h * vec3(grid.dim.x,grid.dim.y,grid.dim.z));
    m_nodes = new ParticleGridNode[m_grid.nodeCount()];
    //m_densities = new float[m_grid.nodeCount()];
    //m_albedo = new float[m_grid.nodeCount()];
}

void MitsubaExporter::test(float t)
{
    // can we draw a sphere?
    float v = m_grid.h * m_grid.h * m_grid.h;

    // draw a cloudy sphere in center of bbox, bobbing up and down
    float r = m_bbox.width()*.25;
    float r2 = r*r;

    float sx = m_bbox.center().x;
    float sy = m_bbox.center().y + sin(t*10)*m_bbox.height();
    //float sy = .25;
    float sz = m_bbox.center().z;

    float x,y,z;
    float dx,dy,dz;
    float mass;

    float minX = m_bbox.min().x;
    float minY = m_bbox.min().y;
    float minZ = m_bbox.min().z;

    // looping over OUR grid format - x major, z minor
    for (size_t i=0; i<m_grid.nodeCount(); ++i)
    {
        // looping over OUR grid format - xmajor, zminor
        int j = i;
        // data[((zpos*yres + ypos)*xres + xpos)*channels + chan]
        int xpos = j / (m_grid.dim.y*m_grid.dim.z);
        j -= xpos*(m_grid.dim.y*m_grid.dim.z);
        int ypos = j / m_grid.dim.z;
        j -= ypos*m_grid.dim.z;
        int zpos = j;

        // true grid positions in simulation space
        x = minX + m_grid.h * xpos;
        y = minY + m_grid.h * ypos;
        z = minZ + m_grid.h * zpos;
        dx=x-sx;
        dy=y-sy;
        dz=z-sz;
        if (dx*dx+dy*dy+dz*dz < r2)
        {
            mass=v;
        }
        else
        {
            mass=0;
        }

        m_nodes[i].mass = mass;
    }
}

void MitsubaExporter::exportVolumeData(float t)
{
/// the engine has to decide to copy over ParticleGrid data
/// at some point to the CPU. right now this graph traversal wont do.
/// since sceneNodes dont have pointers to the particleGrids.
    m_busy = true;
//    for (int i=0; i<m_grid.nodeCount();i++)
//    {
//        m_densities[i]=std::sin(float(m_frame * i)/50);
//    }

    QString fname = QString("%1_%2.vol").arg(m_fileprefix, QString("%1").arg(m_frame,4,'d',0,'0'));
    std::ofstream os(fname.toStdString().c_str());

    os.write("VOL", 3);
    char version = 3;
    os.write((char *) &version, sizeof(char));
    int value = 1;
    os.write((char *) &value, sizeof(int)); //Dense float32-based representation
    os.write((char *) &m_grid.dim.x, sizeof(int));
    os.write((char *) &m_grid.dim.y, sizeof(int));
    os.write((char *) &m_grid.dim.z, sizeof(int));
    int channels = 1;      // number of channels
    os.write((char *) &channels, sizeof(int));

//    float minX = m_bbox.min().x;
//    float minY = m_bbox.min().y;
//    float minZ = m_bbox.min().z;
//    float maxX = m_bbox.max().x;
//    float maxY = m_bbox.max().y;
//    float maxZ = m_bbox.max().z;

    // the bounding box of the snow volume
    // corresponds to exactly where the heterogenous medium
    // will be positioned.
    float minX=-.5;
    float maxX=.5;
    float minY=0;
    float maxY=1;
    float minZ=-.5;
    float maxZ=.5;

    // bounding box
    os.write((char *) &minX, sizeof(float));
    os.write((char *) &minY, sizeof(float));
    os.write((char *) &minZ, sizeof(float));
    os.write((char *) &maxX, sizeof(float));
    os.write((char *) &maxY, sizeof(float));
    os.write((char *) &maxZ, sizeof(float));

    float h = m_grid.h;
    float v = h*h*h;

    int xres,yres,zres;
    xres = m_grid.dim.x;
    yres = m_grid.dim.y;
    zres = m_grid.dim.z;

    float x,y,z;
    float dx,dy,dz;
    float sx,sy,sz;

    sx = m_grid.pos.x + m_grid.dim.x * h/2;
    sy = m_grid.pos.y + m_grid.dim.y * h/2; // center of simulation box
    sz = m_grid.pos.z + m_grid.dim.z * h/2;
    printf("sim box center : %f, %f, %f \n", sx, sy, sz);
    // data[((zpos*yres + ypos)*xres + xpos)*channels + chan]
    float r2 = .2 * .2;
    for (size_t i=0; i<m_grid.nodeCount(); ++i) {
        // for each mitsuba-ordered index (z-major,x-minor), find the index of OUR grid
        // that it corresponds to, then write it.

        int j=i;
        int xpos = j%xres;
        j = (j-xpos)/xres;
        int ypos = j%yres;
        j = (j-ypos)/yres;
        int zpos = j;

        // index for our grid indexing scheme
        //int index = (xpos*yres + ypos)*zres + zpos;
        //float density = m_nodes[index].mass/v;

        x = m_grid.pos.x + xpos*h;
        y = m_grid.pos.y + ypos*h;
        z = m_grid.pos.z + zpos*h;
        dx = x-sx;
        dy = y-sy;
        dz = z-sz;

        //float density = (dx*dx + dy*dy + dz*dz < r2) ? 1.f : 0.f;
        //float density = sin(z*20);
        float density = 0;
        if (dx*dx + dy*dy + dz*dz < r2)
        {
            //printf("success\n");
            density = 1;
        }

        os.write((char *) &density, sizeof(float));
    }
    os.close();

    m_lastUpdateTime = t;
    m_frame += 1;
    m_busy=false;
}


ParticleGridNode * MitsubaExporter::getNodesPtr()
{
    return m_nodes;
}

/// temporarily put on hold - wait for the rest of the scenegraph implementation
/// to be frozen.

//void MitsubaExporter::exportScene(QString fprefix1, int frame) //, Engine *engine, Camera * camera
//{
    /**
     * Exports Single Frame of Scene to Mitsuba format.
     *
     * Traverses the scene graph and converts colliders to basic Mitsuba primitives
     * filled objects are loaded OBJs, and the snow is a heterogenous medium-type volume data
     *
     */
    //QString fname = fprefix + QString("_") + QString::number(frame,);
//    QString fprefix2 = QString("%1_%2").arg(fprefix1, QString("%1").arg(frame,4,'d',0,'0'));
//    exportVolumeData(fprefix2);
//    std::cout << "file written!" << std::endl;


//    // xml header
//    QDomProcessingInstruction pi = m_document.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\" ");
//    m_document.appendChild(pi);
//    // root element for the scene
//    QDomElement sceneNode = m_document.createElement("scene");
//    sceneNode.setAttribute("version", "0.5.0");
//    m_document.appendChild(sceneNode);

//    // we want a volumetric path tracer
//    appendRenderer(sceneNode);
//    // add the camera
//    appendCamera(sceneNode, camera);

//    // now traverse the scene graph for renderables.
//    // renderables are either snow containers
//    // or colliders.
//    Scene * scene = engine->scene();
//    QList<SceneNode *> nodes = scene->root()->allNodes();
//    for (int i=0;i<nodes.size();++i)
//    {
//        appendShape(sceneNode,nodes[i]);
//    }

//    // write it to file
//    QFile file(fname);
//    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
//    {
//        std::cout << "write failed" << std::endl;
//    }
//    else
//    {
//        QTextStream stream(&file);
//        stream << m_document.toString();
//        file.close();
//        std::cout << "file written" << std::endl;
//    }

//}


//QDomElement MitsubaExporter::appendCamera(QDomElement node, Camera *camera)
//{
//    //    <sensor type="perspective">
//    //		<float name="focusDistance" value="1.25668"/>
//    //		<float name="fov" value="45.8402"/>
//    //		<string name="fovAxis" value="x"/>
//    //		<transform name="toWorld">
//    //			<scale x="-1"/>

//    //			<lookat target="-0.166029, 0.148984, -0.537402" origin="-0.61423, 0.154197, -1.43132" up="-0.000640925, -0.999985, -0.0055102"/>
//    //		</transform>

//    //		<sampler type="ldsampler">
//    //			<integer name="sampleCount" value="64"/>
//    //		</sampler>

//    //		<film type="hdrfilm">
//    //			<integer name="height" value="576"/>
//    //			<integer name="width" value="768"/>

//    //			<rfilter type="gaussian"/>
//    //		</film>
//    //	</sensor>


//    //    QDomElement sensorNode = document.createElement("sensor");
//    //    sensorNode.setAttribute("type","perspective"); {
//    //        QDomElement fd = document.createElement("float");
//    //        fd.setAttribute("focusDistance", "1.25668");
//    //        sensorNode.appendChild(fd);

//    //        QDomElement fov = document.createElement("float");
//    //        fov.setAttribute("fov", "45.8402");
//    //        sensorNode.appendChild(fov);

//    //        QDomElement fovAxis = document.createElement("string");
//    //        fovAxis.setAttribute("fovAxis","x");
//    //        sensorNode.appendChild(fovAxis);

//            // transform node
//            // sampler node
//            // film node

//    //    }
//    //    sceneNode.appendChild(sensorNode);
//}

//QDomElement MitsubaExporter::appendLight(QDomElement node)
//{
//    // add some lights to the scene

////    <shape type="obj">
////		<!-- Shiny floor -->
////		<string name="filename" value="plane.obj"/>

////		<bsdf type="diffuse">
////			<rgb name="diffuseReflectance" value=".2, .2, .3"/>
////		</bsdf>
////		<transform name="toWorld">
////			<translate y=".48"/>
////		</transform>
////	</shape>


////    <shape type="sphere">
////		<point name="center" x="0" y="-2" z="-1"/>
////		<float name="radius" value=".2"/>

////		<emitter type="area">
////			<spectrum name="radiance" value="400"/>
////		</emitter>
////	</shape>

//}

//QDomElement MitsubaExporter::appendMedium(QDomElement node)
//{

//    //    <medium type="heterogeneous" id="smoke">
//    //		<string name="method" value="woodcock"/>

//    //		<volume name="density" type="gridvolume">
//    //			<string name="filename" value="smoke.vol"/>
//    //		</volume>

//    //		<volume name="albedo" type="constvolume">
//    //			<spectrum name="value" value="0.9"/>
//    //		</volume>
//    //		<float name="scale" value="100"/>
//    //	</medium>


//    //    <shape type="obj">
//    //		<string name="filename" value="bounds.obj"/>

//    //		<ref name="interior" id="smoke"/>
//    //	</shape>

//}

//QDomElement MitsubaExporter::appendShape(QDomElement node, SceneNode * sceneNode)
//{
//    // appends renderables stored in a sceneNode (but ignores children!)

//QDomElement MitsubaExporter::appendShape(QDomElement node, SceneNode * sceneNode)
//{
    // appends renderables stored in a sceneNode (but ignores children!)

    /*
     *
     * Currently, sceneNodes can have multiple renderables attached. ImplicitColliders
     * are renderables with implicit shape types enumerated. This presents some difficulty when
     * figuring out where to put the obj string attribute - in sceneNode or renderables?
     * as a temporary solution, the obj attribute will be put in a sceneNode and we are enforcing that
     * implicitcolliders are sitting in their own scenenode (the obj attribute is added upon collider creation)
     */
    // append <shape> to scene
//    QDomElement s = appendOBJ( node, sceneNode->getObjFile() );

//    // add transformation to the shape node
//    appendXform(s, sceneNode->getCTM());

//    // add default material to the shape node
//    appendBSDF(s);

//    if (sceneNode->getType() == IMPLICIT_COLLIDER)
//    {
//        // TODO
//    }
//    else if (sceneNode->getType() == SNOW_CONTAINER)
//    {
//        // TODO
//    }
//}

//    /*
//     *
//     * Currently, sceneNodes can have multiple renderables attached. ImplicitColliders
//     * are renderables with implicit shape types enumerated. This presents some difficulty when
//     * figuring out where to put the obj string attribute - in sceneNode or renderables?
//     * as a temporary solution, the obj attribute will be put in a sceneNode and we are enforcing that
//     * implicitcolliders are sitting in their own scenenode (the obj attribute is added upon collider creation)
//     */
//    // append <shape> to scene
//    QDomElement s = appendOBJ(node, sceneNode->getObjFile());

//    // add transformation to the shape node
//    appendXform(s, sceneNode->getCTM());

//    // add default material to the shape node
//    appendBSDF(s);

//    if (sceneNode->getType() == IMPLICIT_COLLIDER)
//    {
//        // TODO
//    }
//    else if (sceneNode->getType() == SNOW_CONTAINER)
//    {
//        // TODO
//    }
//}

//QDomElement MitsubaExporter::appendOBJ(QDomElement node, QString objfile)
//{
//    QDomElement s = m_document.createElement("shape");
//    s.setAttribute("type", "obj");
//    QDomElement str = m_document.createElement("string");
//    str.setAttribute("name","filename");
//    str.setAttribute("value",objfile);
//    s.appendChild(str);
//}

//QDomElement MitsubaExporter::appendBSDF(QDomElement node)
//{
//    QDomElement bsdf = m_document.createElement("bsdf");
//    bsdf.setAttribute("type","diffuse");
//    QDomElement rgb = m_document.createElement("rgb");
//    rgb.setAttribute("name","diffuseReflectance");
//    rgb.setAttribute("value",".2, .2, .3"); // dull gray color
//    bsdf.appendChild(rgb);
//    node.appendChild(bsdf);
//    return bsdf;
//}

//QDomElement MitsubaExporter::appendRenderer(QDomElement node)
//{
//    //    <integrator type="volpath_simple">
//    //		<integer name="maxDepth" value="8"/>
//    //	</integrator>
//        // integrator settings
//    QDomElement integrator = m_document.createElement("integrator");
//    integrator.setAttribute("type","volpath_simple");
//    QDomElement in = m_document.createElement("integer");
//    in.setAttribute("name", "maxDepth");
//    in.setAttribute("value","8");
//    integrator.appendChild(in);
//    node.appendChild(integrator);
//    return integrator;
//}

//QDomElement MitsubaExporter::appendXform(QDomElement node, glm::mat4 xform)
//{
//    QDomElement t = m_document.createElement("transform");
//    t.setAttribute("name", "toWorld");
//    QDomElement m = m_document.createElement("matrix");
//    // convert matrix

//    char buf[16*sizeof(float)];
//    sprintf(buf, "%f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",xform[0][0],xform[1][0],xform[2][0],xform[3][0],
//                                                                     xform[0][1],xform[1][1],xform[2][1],xform[3][1],
//                                                                     xform[0][2],xform[1][2],xform[2][2],xform[3][2],
//                                                                     xform[0][3],xform[1][3],xform[2][3],xform[3][3]);
//    QString matstr(buf);
//    m.setAttribute("value", matstr);

//    t.appendChild(m);
//    node.appendChild(t);

//    return t;
//}
