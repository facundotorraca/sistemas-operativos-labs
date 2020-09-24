# Lab: shell

### Búsqueda en $PATH

#### ¿cuáles son las diferencias entre la syscall execve(2) y la familia de wrappers proporcionados por la librería estándar de C (libc) exec(3)?

La familia exec(3) son un conjunto de wrappers de la syscall execve.

const char *pathname: Un string con la direccion y nombre del ejecutable
char *const argv[]: Un vector terminado en NULL con los argumentos del programa
char *const envp[]: Un vector terminado en NULL con las variables de entorno del programa

La familia exec(3) provee una serie wrappers a la syscall execve para simplificar su uso

execl; execlp; execle; execv; execvp; execvpe.

Las funciones que poseen un 'l' (execl, execlp, execle) envian los argumentos como una lista
de parametros terminada en NULL (char* arg1, ... ,char argN, NULL).

Las funciones que poseen un 'v' (execv, execvp, execvpe) envian loas argumentos como un vector
de parametros terminado en NULL (char* const argv[] donde el ultimo elemento es NULL).

Las funciones que poseen un 'p' (execlp, execvp, execvpe) buscan el ejecutable en caso de que no
se pase un path (el string no contiene '\'). El path es buscado

Las funciones que poseen un 'e' (execle, execvpe) envian ademas un vector con las variables de
entorno para la ejecucion del programa pasado por parametro (char* const eargv[]).

En resumen, las funciones de la familia exec(3) son un wrapper que internamente realizan un llamado
a execve(2), pero proveen un front-end para que sea mas facil su utilizacion (por ejemplo, buscar
el archvio ejecutable si no se le pasa un path)

### Comandos built-in:

#### Pregunta: ¿entre cd y pwd, alguno de los dos se podría implementar sin necesidad de ser built-in? ¿por qué? ¿cuál es el motivo, entonces, de hacerlo como built-in? (para esta última pregunta pensar en los built-in como true y false)

El comando cd claramente se debe implementar como un built-in ya que debe modificar directamente el proceso en cual esta corriedno la shell, por lo que no tendria sentido que no se implemente de esta manera.

El comando pwd por su parte podria ser implementado sin ser un built-in, ya que es posible que un proceso obtenga el path de donde fue llamado.

El motivo de hacer algunos comando built-in es evitar la creacion de un nuevo proceso para algunas funcionalidades que se utilizan con frecuencia, ya que la creacion de un nuevo proceso es bastante costosa.

### Variables de entorno adicionales

#### ¿por qué es necesario hacer el llamado a setenv(3) luego de la llamada a fork(2)?

Porque setenv agrega las variables de entorno al proceso en que fue llamada. Si no se hiciera dentro del proceso hijo, es decir, luego del fork, las variables de entorno no solo se agregarian al hijo, pero tambien a la shell, lo cual no es el objetivo.
Las variables tambien se agregarian al hijo ya que se clonarian las ariables de entorno de la shell.

#### Supongamos, entonces, que en vez de utilizar setenv(3) por cada una de las variables, se guardan en un array y se lo coloca en el tercer argumento de una de las funciones de exec(3). ¿el comportamiento es el mismo que en el primer caso? Explicar qué sucede y por qué. Describir brevemente (sin implementar) una posible implementación para que el comportamiento sea el mismo.

No. Al pasarle de forma explicita las variables de entorno a alguna de las funciones de la familia exec(3) (execle, execvpe) o directamente a la syscall execve, hace que el nuevo proceso solo tenga acceso a esas variables definidas por el programador, y no todas las definidas en la constante global 'environ' (extern char **environ)

Una posible implementacion seria cargar en un array con las variables definidas en 'environ' y luego agregar las nuevas variables de entorno definidas al vector. De esa manera el nuevo proceso tendria acceso a las globales (env) y a las definidas por el usuario.

### Procesos en segundo plano

#### Detallar cúal es el mecanismo utilizado para implementar los procesos en segundo plano

La implementacion de los procesos en segundo plano es muy similar a la implementacion de una
ejecucion normal en primer plano, a diferencia que no se espera a que el proceso lanzado termine.
Esto se logra lanzando un proceso (mediante la syscall fork) y obviando la espera de que termine la ejecucion, es decir, se vuelve directamente a la seccion que toma nuevos comandos.

El proceso lanzado, al terminar queda en modo 'zombie'. Este estado indica que el proceso termino pero nunca fue "atrapado" por el proceso padre. Para evitar esto, antes de ejecutar un nuevo comando se hace un llamado a la syscall waitpid con el parametro pid igual a 0 (haciendo que atrape a cualquier proceso) y el flag WNOHANG, que evita que el proceso principal quede esperando.

Para atrapar a todos los procesos zombies (ya que pueden haberse lanzado mas de un proceso en segundo plano), el llamdo a waitpid se coloca dentro de un ciclo while y se ejecuta mientras la syscall devuelva un pid mayor a 0 (indicando que atrapo un proceso zombie).

### Flujo estándar

#### Investigar el significado del tipo de redireccion 2>&1 y explicar qué sucede con la salida de cat out.txt. Comparar con los resultados obtenidos anteriormente.

Este tipo de redireccion indica que se quiere redireccionar la salida estandar de error (2>) a la
que apunta (&1). En este caso, al agregar el '&' adelante, se toma al numero 1 como un file descriptor. En resumen, estamos indicando que se debe redireccionar stderr al mismo archivo que se esta redireccionando stdin.

La salida en mi computadora es del comando: $ ls -C /home /noexiste >out.txt 2>&1

ls: cannot access '/noexiste': No such file or directory
/home:
facutorraca

La primer linea corresponde a un error, que se redirecciono de stderr al archivo out.txt
El error consiste en que no se encuentra el directorio "noexiste"

La segunda y tercer linea corresponde a la salida estandar del programa (stdout) que tambien fue redireccionada al archivo out.txt

Es decir, a diferencia del caso anterior ($ ls -C /home /noexiste >out.txt 2>&1), en este caso, tanto stderr como stdout se redirecciono al mismo archivo.

#### Challenge: investigar, describir funcionalidad del operador de redirección >> y &>

El operador >> hace lo mismo que el operador > pero si el archivo existe, adjunta la salida al final del archivo.

El operado &> hace lo mismo que el operador 2>&1 pero si el archivo existe, adjunta la salida al final del archivo.

### Tuberías simples (pipes)

#### Investigar y describir brevemente el mecanismo que proporciona la syscall pipe(2), en particular la sincronización subyacente y los errores relacionados.

La syscall pipe provee dos file descriptors que forman un canal unidireccional, es decir, uno para entrada/escritura y otro para salida/lectura, que permiten la comunicacion entre procesos.

La informacion enviada mediante los pipes es almacenada en el kernel en un buffer de 4096 bytes de tamano.

Si un proceso intenta leer desde un pipe vacio, la funcion read es bloqueante hasta que exista informacion disponible para leer. La escritura nunca es bloqueante siempre y cuando exista lugar en el buffer, caso contrario, se bloquea hasta que se libere el espacio necesario.

La comunicacion se da por medio de un stram de bytes.

Si todos los file descriptors que refieren a la terminal de lectura han sido cerrado, entonces write(2) genera una senal SIGPIPE, que en caso de ignorada, genera el error EPIPE.

### Pseudo-variables

#### Investigar al menos otras dos variables mágicas estándar, y describir su propósito.

* $$: Se expande al process ID (PID) en que esta corriendo la shell
* $!: Se expande al process ID (PID) del ultimo comando que fue corrido en segundo plano.
* $0: Se expande al nombre de la shell (ej: bash)

### Cambios respecto al esqueleto original

* En el archivo readline se realiza un tratamiento especial a los caracteres que son espacios entre comandos (ej ls       -l) de manera que se pueda interpretar como un comando valido (ej ls -l). Tambien se eliminan todos los espacios iniciales de un comando, permitiendo que el comando sea valido
incluso si no escrito desde el principio de la linea (ej: $        cd).

* Se imprime el status del ultimo comando del pipe. Para evitar un tratamiento especial, la seccions 'scmd' de un PIPE contiene el string 'pipe command'
