#ifndef HEADER_F67D53FD621D8D05
#define HEADER_F67D53FD621D8D05

#ifdef __GNUG__
// #pragma interface
#endif

#include <vector>

#include <wx/dynarray.h>
#include <wx/txtstrm.h>

#ifndef __BLOCKS_H__
#define __BLOCKS_H__

class NassiBrickVisitor;
class NassiBrick
{
public:
    NassiBrick();
    NassiBrick(const NassiBrick &rhs);
private:
    NassiBrick &operator=(const NassiBrick &rhs);
public:
    virtual ~NassiBrick();
    virtual NassiBrick *Clone() const = 0;// {return new NassiBrick(*this); }

    NassiBrick *GetPrevious() const { return previous; }
    NassiBrick *GetNext() const { return mNext; }
    NassiBrick *GetParent() const { return parent; }
    NassiBrick *SetNext(NassiBrick *nex);
    NassiBrick *SetPrevious(NassiBrick *prev);
    NassiBrick *SetParent(NassiBrick *brick);
    virtual wxUint32 GetChildCount() const { return 0;}
    virtual NassiBrick *GetChild(wxUint32 /*n*/ = 0) const { return nullptr;}
    virtual NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0);
    virtual void RemoveChild(wxUint32 /*pos*/) { return; }//only for switch elsif blocks
    virtual void AddChild(wxUint32 /*pos*/) { return; }//only for switch elsif blocks

    virtual void SetTextByNumber(const  wxString &str, wxUint32 n = 0) = 0;
    virtual const wxString *GetTextByNumber(wxUint32 n=0) const = 0;

    virtual void accept(NassiBrickVisitor *visitor)=0;

    void GenerateStrukTeX(wxString &str);
    virtual void GetStrukTeX(wxString &str, wxUint32 n);
    virtual void SaveSource(wxTextOutputStream &text_stream, wxUint32);
    virtual bool IsBlock(){return false;}
    bool IsParent(NassiBrick *brick);
    wxUint32 GetLevel();
    bool IsYoungerSibling(NassiBrick *brick);
    bool IsOlderSibling(NassiBrick *brick);
    bool IsSibling(NassiBrick *brick);
protected:
    void SaveCommentString(wxTextOutputStream &text_stream, const wxString &str, wxUint32 k);
    void SaveSourceString(wxTextOutputStream &text_stream,  const wxString &str, wxUint32 k);
    //void SaveFile(wxTextOutputStream &text_stream, wxUint32 n);
    //void SaveString(wxTextOutputStream &text_stream, wxUint32 n, wxUint32 k, bool FillFirst);
private:
    NassiBrick *previous, *mNext, *parent;
public:
    static wxOutputStream &SerializeString(wxOutputStream &stream, wxString str);
    static wxInputStream &DeserializeString(wxInputStream &stream, wxString &str);

protected:
    wxString Source;
    wxString Comment;

public:
    static NassiBrick *SetData(wxInputStream &stream);
    virtual wxOutputStream &Serialize(wxOutputStream &stream)=0;
    virtual wxInputStream &Deserialize(wxInputStream &stream)=0;
};
class NassiInstructionBrick : public NassiBrick
{
public:
    NassiInstructionBrick();
    NassiInstructionBrick(const NassiInstructionBrick &rhs);
private:
    NassiInstructionBrick &operator=(const NassiInstructionBrick &rhs);
public:
    virtual ~NassiInstructionBrick();
    NassiBrick *Clone() const override
    {
        return new NassiInstructionBrick(*this);
    }

    wxUint32 GetChildCount() const override {return 0;}
    NassiBrick *GetChild(wxUint32 /*n*/ = 0) const override {return nullptr;}
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n=0) const override;
    void accept(NassiBrickVisitor *visitor) override;
    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiBreakBrick : public NassiBrick
{
public:
    NassiBreakBrick();
    NassiBreakBrick(const NassiBreakBrick &rhs);
private:
    NassiBreakBrick &operator=(const NassiBreakBrick &rhs);
public:
    virtual ~NassiBreakBrick();
    NassiBrick *Clone() const override
    {
        return new NassiBreakBrick(*this);
    }

    wxUint32 GetChildCount() const  override{return 0;}
    NassiBrick *GetChild(wxUint32 /*n*/ = 0) const override {return nullptr;}
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;
    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiContinueBrick : public NassiBrick
{
public:
    NassiContinueBrick();
    NassiContinueBrick(const NassiContinueBrick &rhs);
private:
    NassiContinueBrick &operator=(const NassiContinueBrick &rhs);
public:
    virtual ~NassiContinueBrick();
    NassiBrick *Clone() const override
    {
        return new NassiContinueBrick(*this);
    }

    wxUint32 GetChildCount() const override
    {
        return 0;
    }
    NassiBrick *GetChild(wxUint32 /*n*/ = 0) const override
    {
        return nullptr;
    }
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n=0) const override;
    void accept(NassiBrickVisitor *visitor) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiReturnBrick : public NassiBrick
{
public:
    NassiReturnBrick();
    NassiReturnBrick(const NassiReturnBrick &rhs);
private:
    NassiReturnBrick &operator=(const NassiReturnBrick &rhs);
public:
    virtual ~NassiReturnBrick();
    NassiBrick *Clone() const override {return new NassiReturnBrick(*this);}
    wxUint32 GetChildCount() const override {return 0;}
    NassiBrick *GetChild(wxUint32 /*n*/ = 0) const override {return nullptr;}
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiIfBrick : public NassiBrick
{
public:
    NassiIfBrick();
    NassiIfBrick(const NassiIfBrick &rhs);
private:
    NassiIfBrick &operator=(const NassiIfBrick &rhs);
public:
    virtual ~NassiIfBrick();
    NassiBrick *Clone() const override
    {
        return new NassiIfBrick(*this);
    }

    wxUint32 GetChildCount() const override
    {
        return(2);
    }
    NassiBrick *GetChild(wxUint32 n = 0) const override;
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
private:
    NassiBrick *TrueChild;
    NassiBrick *FalseChild;
    wxString TrueSourceText, TrueCommentText;
    wxString FalseSourceText, FalseCommentText;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiForBrick : public NassiBrick
{
public:
    NassiForBrick();
    NassiForBrick(const NassiForBrick &rhs);
private:
    NassiForBrick &operator=(const NassiForBrick &rhs);
public:
    virtual ~NassiForBrick();
    NassiBrick *Clone() const override
    {
        return new NassiForBrick(*this);
    }

    wxUint32 GetChildCount() const override {return 1;}
    NassiBrick *GetChild(wxUint32 n = 0) const override;
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
private:
    NassiBrick *Child;
    wxString InitSourceText, InitCommentText;
    wxString InstSourceText, InstCommentText;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiWhileBrick : public NassiBrick
{
public:
    NassiWhileBrick();
    NassiWhileBrick(const NassiWhileBrick &rhs);
private:
    NassiWhileBrick &operator=(const NassiWhileBrick &rhs);
public:
    virtual ~NassiWhileBrick();
    NassiBrick *Clone() const override
    {
        return new NassiWhileBrick(*this);
    }

    wxUint32 GetChildCount() const override
    {
        return 1;
    }
    NassiBrick *GetChild(wxUint32 n = 0) const override;
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
private:
    NassiBrick *Child;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiBlockBrick : public NassiBrick
{
public:
    NassiBlockBrick();
    NassiBlockBrick( const NassiBlockBrick &rhs);
private:
    NassiBlockBrick &operator=(const NassiBlockBrick &rhs);
public:
    ~NassiBlockBrick();
    NassiBrick *Clone() const override
    {
        return new NassiBlockBrick(*this);
    }
    wxUint32 GetChildCount() const override
    {
        return 1;
    }
    NassiBrick *GetChild(wxUint32 n = 0) const override;
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
    bool IsBlock() override {return true;}
private:
    NassiBrick *Child;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};
class NassiDoWhileBrick : public NassiBrick
{
public:
    NassiDoWhileBrick();
    NassiDoWhileBrick(const NassiDoWhileBrick &rhs);
private:
    NassiDoWhileBrick &operator=(const NassiDoWhileBrick &rhs);
public:
    virtual ~NassiDoWhileBrick();
    NassiBrick *Clone() const override
    {
        return new NassiDoWhileBrick(*this);
    }

    wxUint32 GetChildCount() const override
    {
        return 1;
    }
    NassiBrick *GetChild(wxUint32 n = 0) const override;
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
private:
    NassiBrick *Child;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};

typedef std::vector<NassiBrick*> ArrayOfNassiBrickPtrs;
class NassiSwitchBrick : public NassiBrick
{
public:
    NassiSwitchBrick();
    NassiSwitchBrick(const NassiSwitchBrick &rhs);
private:
    NassiSwitchBrick &operator=(const NassiSwitchBrick &rhs);
public:
    virtual ~NassiSwitchBrick();
    NassiBrick *Clone() const override { return new NassiSwitchBrick(*this); }

    wxUint32 GetChildCount() const override;
    NassiBrick *GetChild(wxUint32 n = 0) const override;
    NassiBrick *SetChild(NassiBrick *brick, wxUint32 n = 0) override;
    void SetTextByNumber(const  wxString &str, wxUint32 n = 0) override;
    const wxString *GetTextByNumber(wxUint32 n = 0) const override;
    void accept(NassiBrickVisitor *visitor) override;
    void RemoveChild(wxUint32 pos) override;
    void AddChild(wxUint32 pos) override;

    void GetStrukTeX(wxString &str, wxUint32 n) override;
    void SaveSource(wxTextOutputStream &text_stream, wxUint32 n = 0) override;
private:
    wxUint32 nChilds;
    std::vector<wxString *> Comments;
    std::vector<wxString *> Sources;
    ArrayOfNassiBrickPtrs childBlocks;
    void Destructor();
    static wxString EmptyString;
public:
    wxOutputStream &Serialize(wxOutputStream &stream) override;
    wxInputStream &Deserialize(wxInputStream &stream) override;
};

class NassiBricksCompositeIterator
{
public:
    NassiBricksCompositeIterator(NassiBrick *frst);
    void First();
    bool IsDone() { return done; }
    NassiBrick *CurrentItem() { return current; }
    void Next();
private:
    NassiBrick *first;
    NassiBrick *current;
    NassiBrick *currentParent;
    wxUint32 child;
    bool done;
    NassiBricksCompositeIterator *itr;
private:
    void SetNext();
    bool SetItrNextChild();
};

#endif

#endif // header guard
