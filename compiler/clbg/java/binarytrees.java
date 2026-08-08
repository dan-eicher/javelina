/* The Computer Language Benchmarks Game
   https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   contributed by Jarkko Miettinen
   *reset*

   PORT: the published binarytrees javaxint-2 program with its nested
   `private static class TreeNode` hoisted to a top-level class in the same
   compilation unit. Nested classes are Java 1.1; multiple top-level classes in
   one file are 1.0. The algorithm, the allocation pattern and the output are
   unchanged.
*/

public class binarytrees {

   private final static int minDepth = 4;

   public static void main(String[] args){
      int n = 0;
      if (args.length > 0) n = Integer.parseInt(args[0]);

      int maxDepth = (minDepth + 2 > n) ? minDepth + 2 : n;
      int stretchDepth = maxDepth + 1;

      int check = (TreeNode.bottomUpTree(stretchDepth)).itemCheck();
      System.out.println("stretch tree of depth "+stretchDepth+"\t check: " + check);

      TreeNode longLivedTree = TreeNode.bottomUpTree(maxDepth);

      for (int depth=minDepth; depth<=maxDepth; depth+=2){
         int iterations = 1 << (maxDepth - depth + minDepth);
         check = 0;

         for (int i=1; i<=iterations; i++){
            check += (TreeNode.bottomUpTree(depth)).itemCheck();
         }
         System.out.println(iterations + "\t trees of depth " + depth + "\t check: " + check);
      }
      System.out.println("long lived tree of depth " + maxDepth + "\t check: "+ longLivedTree.itemCheck());
   }
}

class TreeNode
{
   private TreeNode left, right;

   static TreeNode bottomUpTree(int depth){
      if (depth>0){
         return new TreeNode(
               bottomUpTree(depth-1)
               , bottomUpTree(depth-1)
         );
      }
      else {
         return new TreeNode(null,null);
      }
   }

   TreeNode(TreeNode left, TreeNode right){
      this.left = left;
      this.right = right;
   }

   int itemCheck(){
      // if necessary deallocate here
      if (left==null) return 1;
      else return 1 + left.itemCheck() + right.itemCheck();
   }
}
