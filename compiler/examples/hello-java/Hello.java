// A plain Java program. javelinac compiles it to a WebAssembly module; the
// javelina engine runs it. Nothing here is javelina-specific.
public class Hello {
    public static void main(String[] args) {
        System.out.println("Hello from javelina!");
        int sum = 0;
        for (int i = 1; i <= 10; i++) sum += i;
        System.out.println("sum(1..10) = " + sum);
    }
}
