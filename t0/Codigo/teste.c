#include <stdio.h>
#include <stdlib.h>

#include "gerador.h"

int main(){
	gerador_t *gerador = gerador_cria();

	printf("%d %d %d\n", gerador_numero(gerador), gerador_numero(gerador), gerador_numero(gerador));

	return 0;
}
