package datastructure;

public interface Visitor
{
    void visit(Object object);
    
    boolean isDone();
}

