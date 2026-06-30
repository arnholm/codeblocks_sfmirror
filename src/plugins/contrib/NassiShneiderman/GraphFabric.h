
#ifndef __CreateGraphVisitor_h__
#define __CreateGraphVisitor_h__

#include "NassiBrickVisitor.h"
#include "GraphBricks.h"

class NassiView;
class GraphNassiBrick;
class GraphFabric  : public NassiBrickVisitor
{
public:
    GraphFabric(NassiView *view, BricksMap *map);
private:
    GraphFabric(const GraphFabric &p);
    GraphFabric &operator=(const GraphFabric &rhs);
public:
    virtual ~GraphFabric();
    //void Visit(NassiBrick *brick) override;
    void Visit(NassiInstructionBrick *brick) override;
    void Visit(NassiIfBrick *brick) override;
    void Visit(NassiWhileBrick *brick) override;
    void Visit(NassiDoWhileBrick *brick) override;
    void Visit(NassiSwitchBrick *brick) override;
    void Visit(NassiBreakBrick *brick) override;
    void Visit(NassiContinueBrick *brick) override;
    void Visit(NassiReturnBrick *brick) override;
    void Visit(NassiForBrick *brick) override;
    void Visit(NassiBlockBrick *brick) override;
public:
    GraphNassiBrick *CreateGraphBrick(NassiBrick *brick);

private:
    GraphNassiBrick *gbrick;
    NassiView *m_view;
    BricksMap *m_map;
};

#endif //__CreateGraphVisitor_h__

