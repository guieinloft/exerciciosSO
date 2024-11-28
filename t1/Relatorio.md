# 1. Introdução:
Um sistema operacional é um software capaz de gerenciar recursos de um dispositivo, como memória e processamento, controlar a execução de programas e processos, definir prioridade de execução de um programa, etc. Além disso, ele fornece uma abstração do hardware para o usuário final, conectando os programas aos componentes físicos do computador.

Os primeiros computadores podiam apenas executar código de máquina e precisavam de um ser humano para controlar a execução de processos. Porém, na década de 60, os primeiros sistemas operacionais surgiram. Ao longo do tempo, estes sistemas operacionais foram ganhando novos recursos, como escalonamento, multiprocessamento, gerenciamento de entrada e saída, entre outros.

Neste relatório, será observado a maneira que diferentes algoritmos de escalonamento, com e sem preempção, podem afetar o desempenho do sistema operacional. Também será analisado o impacto de modificar diferentes parâmetros de tempo na velocidade do sistema. 

# 2. Objetivo
Analisar diferentes técnicas de escalonamento de processos e seu impacto no desempenho do sistema operacional. Entender como diferentes configurações podem afetar a velocidade do sistema operacional.

# 3. Descrição do experimento
Foi desenvolvido um sistema operacional rudimentar para a máquina virtual fornecida pelo professor. Neste sistema operacional foram implementados todos os requisitos especificados no arquivo README.md.

## 3.1. Escalonadores
Foram implementados três tipos de escalonadores no sistema operacional.

### Escalonador Simples
Este escalonador escolhe o primeiro processo pronto da tabela de processos. Enquanto o processo corrente pode executar, ele vai continuar executando, não havendo nenhum tipo de preempção. Caso ele não possa mais executar, seja por um bloqueio ou pela morte do mesmo, o próximo processo pronto na tabela de processos será executado.

### Escalonador Circular (Round-robin)
Este escalonador é um escalonador preemptivo que busca distribuir carga igualmente entre processos. Cada processo possui um intervalo de tempo, o _quantum_, que determina o tempo restante que o processo possui para executar. Em cada interrupção de relógio, o quantum do processo corrente diminui. Quando seu quantum chegar a 0, o processo volta ao final da fila de processos e seu _quantum_ é reiniciado.

### Escalonador prioritário
Este escalonador é um escalonador preemptivo semelhante ao escalonador circular. Nele, cada processo possui, além do _quantum_, uma prioridade, que é calculada dinamicamente. Todo processo é criado com prioridade = 0.5 e, quando perde o processador, seja por bloqueio ou por preempção, a sua prioridade é recalculada. O cálculo da prioridade segue a fórmula:
`prio = (prio + t_exec/t_quantum) / 2`
onde `t_exec` é o tempo desde o início da execução do processo.

## 3.2. Interrupções de relógio
O intervalo de interrupção de relógio é o número de instruções que são executadas para que o relógio gere uma interrupção que chama o sistema operacional. O sistema operacional foi executado com o tempo de interrupção de relógio variando entre 25 e 60, usando o escalonador prioritário e o _quantum_ igual a 5.

## 3.3. Quantum
O sistema operacional foi executado com o _quantum_ de cada processo variando entre 1 e 8, usando o escalonador prioritário e o intervalo de interrupção de relógio igual a 50.

# 4. Resultados e discussão
## 4.1. Escalonadores
### Escalonador simples (intervalo = 50):
| N. Processos | T. Execução |  T. Ocioso | N. Preempções |
|-|-|-|-|
| 4 | 27885 | 4347 | 0 |

| N. IRQ Reset | N. IRQ Erro CPU | N. IRQ Chamada Sistema| N. IRQ Relógio | N. IRQ Teclado | N. IRQ Tela |
|-|-|-|-|-|-|
| 1 | 0 | 462 | 556 | 0 | 0 |

| ID | N. Preempções | T. Retorno | T. Resposta | V. Pronto | T. Pronto | V. Exec. | T. Exec. | V. Bloq. | T. Bloq. |
|-|-|-|-|-|-|-|-|-|-|
| 1 | 0 | 27869 | 7 | 4 | 29 | 4 | 760 | 3 | 27080 |
| 4 | 0 | 27162 | 97 | 131 | 12758 | 131 | 9576 | 130 | 4828 |
| 3 | 0 | 15770 | 507 | 21 | 10660 | 21 | 4023 | 20 | 1087 |
| 2 | 0 | 13412 | 560 | 7 | 3925 | 7 | 9179 | 6 | 308 |

O escalonador simples teve o pior desempenho entre os três implementados. Por não possuir preempção, processos em execução não podem ser interrompidos (exceto em caso de bloqueio), o que afeta o desempenho especialmente quando processos demorados são executados primeiro.

### Escalonador circular (intervalo = 50, quantum = 5):
| N. Processos | T. Execução |  T. Ocioso | N. Preempções |
|-|-|-|-|
| 4 | 24049 | 2431 | 58 |

| N. IRQ Reset | N. IRQ Erro CPU | N. IRQ Chamada Sistema| N. IRQ Relógio | N. IRQ Teclado | N. IRQ Tela |
|-|-|-|-|-|-|
| 1 | 0 | 462 | 479 | 0 | 0 |

| ID | N. Preempções | T. Retorno | T. Resposta | V. Pronto | T. Pronto | V. Exec. | T. Exec. | V. Bloq. | T. Bloq. |
|-|-|-|-|-|-|-|-|-|-|
| 1 | 2 | 24033 | 0 | 5 | 0 | 5 | 740 | 2 | 23293 |
| 4 | 5 | 23226 | 99 | 110 | 10909 | 110 | 8537 | 104 | 3880 |
| 2 | 39 | 17095 | 178 | 48 | 8576 | 48 | 8058 | 8 | 461 |
| 3 | 12 | 11984 | 268 | 26 | 6976 | 26 | 4284 | 13 | 725 |

O escalonador circular, por possuir preempção, apresentou um desempenho significativamente melhor que o escalonador simples. Nota-se que os tempos de retorno e resposta de cada processo diminuíram em relação aos tempos do processador simples, fazendo com que os processos ficassem inativos por menos tempo.

### Escalonador prioritário (intervalo = 50, quantum = 5):
| N. Processos | T. Execução |  T. Ocioso | N. Preempções |
|-|-|-|-|
| 4 | 22902 | 1828 | 59 |

| N. IRQ Reset | N. IRQ Erro CPU | N. IRQ Chamada Sistema| N. IRQ Relógio | N. IRQ Teclado | N. IRQ Tela |
|-|-|-|-|-|-|
| 1 | 0 | 462 | 456 | 0 | 0 |

| ID | N. Preempções | T. Retorno | T. Resposta | V. Pronto | T. Pronto | V. Exec. | T. Exec. | V. Bloq. | T. Bloq. |
|-|-|-|-|-|-|-|-|-|-|
| 1 | 2 | 22886 | 0 | 5 | 0 | 5 | 740 | 2 | 22146 |
| 4 | 5 | 22179 | 97 | 101 | 9810 | 101 | 8578 | 95 | 3791 |
| 2 | 39 | 17748 | 190 | 48 | 9124 | 48 | 8163 | 8 | 461 |
| 3 | 12 | 11487 | 269 | 27 | 7284 | 27 | 3593 | 13 | 610 |

O escalonador prioritário apresentou os melhores resultados entre os escalonadores implementados. O escalonador circular atribui a mesma prioridade a todos os processos, porém, ao modificar as prioridades, o sistema garante que processos que executaram por menos tempo (geralmente processos com maior uso de E/S) tenham prioridade no acesso da CPU.


## Interrupções de relógio (escalonador prioritário, quantum = 5):
![Gráfico do tempo de execução e tempo ocioso do sistema operacional em relação ao intervalo de interrupção de relógio](https://raw.githubusercontent.com/guieinloft/exerciciosSO/refs/heads/main/t1/Metricas/gr_intervalo.png)

Percebe-se que o sistema operacional possui melhor desempenho com um intervalo específico de tempo, neste caso sendo 40 instruções, tendo desempenho pior com intervalos menores e maiores que este número. Percebe-se também, que o tempo ocioso do SO diminui em relação ao tempo, porém, ao interromper a execução dos processos frequentemente faz com que muito tempo seja desperdiçado executando o sistema operacional.

## Quantum (escalonador prioritário, int. relógio = 50):
![Gráfico do tempo de execução e tempo ocioso do sistema operacional em relação ao intervalo de interrupção de relógio](https://raw.githubusercontent.com/guieinloft/exerciciosSO/refs/heads/main/t1/Metricas/gr_quantum.png)

Praticamente o mesmo comportamento da mudança de intervalo acontece com a mudança de _quantum_. O sistema operacional possui um desempenho melhor com um número de _quantum_ específico, neste caso 3 interrupções de relógio, tendo pior desempenho com números abaixo ou acima deste. Isto ocorre pois a troca de processos, caso seja efetuada muito frequentemente, gasta recursos de tempo e processamento.

# 5. Conclusão
O sistema operacional foi implementado de acordo com os requisitos apresentados pelo professor.

Os escalonadores preemptivos apresentaram uma melhoria de desempenho sobre o escalonador não-preemptivo, com o escalonador prioritário apresentando o melhor desempenho entre os preemptivos. Isso indica que a preemptividade é benéfica para o desempenho do sistema operacional.

    Entretanto, caso ocorram muitas preempções durante um curto intervalo de tempo, ou caso o sistema operacional interrompa frequentemente a CPU, pode causar um _overhead_ significativo e afetar o tempo de execução do sistema, indicado pelos resultados da mudança de intervalo e de _quantum_.

Portanto, pode-se dizer que a escolha de escalonador, de intervalo de tempo de relógio e de _quantum_ são um aspecto importante a ser considerado na implementação de um sistema operacional. Cabe ao desenvolvedor decidir quais parâmetros são os ideais para o hardware e seus requisitos.

# 6. Referências
TANENBAUM, A. S.; BOS, H. Sistemas Operacionais Modernos, 4th ed. New Jersey: Pearson, 2015.

SISTEMA OPERATIVO. In: WIKIPÉDIA, a enciclopédia livre. Flórida: Wikimedia Foundation, 2024. Disponível em: <[https://pt.wikipedia.org/w/index.php?title=Sistema_operativo&oldid=68588140](https://pt.wikipedia.org/w/index.php?title=Sistema_operativo&oldid=68588140)>. Acesso em: 28 nov. 2024.
