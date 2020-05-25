/**
 * \file src/solution/cplex.cc
 * \brief Contains the \c Algorithm class methods.
 *
 * \authors Rodrigo Alves Prado da Silva \<rodrigo_prado@id.uff.br\>
 * \authors Yuri Frota \<abitbol@gmail.com\>
 * \copyright Fluminense Federal University (UFF)
 * \copyright Computer Science Department
 * \date 2020
 *
 * This source file contains the methods from the \c Cplex class that run the mode of the
 * exact solution.
 */

#include "src/solution/cplex.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <ilcplex/ilocplex.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <string>
// #include "data.h"

// #include <list>
// #include <vector>
// #include <limits>
// #include <cmath>
// #include <memory>
// #include <ctime>
// #include <utility>

#include "src/model/data.h"
#include "src/model/dynamic_file.h"
#include "src/model/static_file.h"

DECLARE_string(cplex_output_file);

#define PRECISAO 0.00001

/**
 * If file reside in the same vm, then the transfer time is 0.0.
 * Otherwise, calculate the transfer time between them using the smallest bandwidth.
 *
 *   \f[
 *      time = \lceil{\min_{storage1.bandwidth(), storage2.bandwidth()} bandwidth}\rceil
 *   \f]
 *
 * Where:
 * \f$ time \f$ is the transfer time between the \c storage1 and \c storage2
 * \f$ storage1.bandwidth() \f$ is the transfer rate of the \c storage1
 * \f$ storage2.bandwidth() \f$ is the transfer rate of the \c storage2
 * \f$ bandwidth \f$ is the minimal transfer rate between the storage1.bandwidth() and storage2.bandwidth()
 *
 * \param[in]  file          File to be transfered
 * \param[in]  storage1      Storage origin/destination
 * \param[in]  storage2      Storage origin/destination
 * \retval     time          The time to transfer \c file from \c file_vm to \c vm with possible
 *                           applied penalts
 */
int ComputeFileTransferTime(File* file, Storage* storage1, Storage* storage2) {
  int time = 0;

  DLOG(INFO) << "Compute the transfer time of File[" << file->get_id() << "] to/from VM["
      << storage1->get_id() << "] to Storage[" << storage2->get_id() << "]";

  // Calculate time
  if (storage1->get_id() != storage2->get_id()) {
    // get the smallest link
    double link = std::min(storage1->get_bandwidth(), storage2->get_bandwidth());
    time = static_cast<int>(std::ceil(file->get_size() / link));
    // time = file->get_size() / link;
  }
  else
  {
    time = 1;
  }


  DLOG(INFO) << "tranfer_time: " << time;
  return time;
}  // double Solution::FileTransferTime(File file, Storage vm1, Storage vm2) {

ILOSTLBEGIN
void Cplex::Run() {
  // bool DEPU = false;

  // nome das variaveis
  char var_name[100];

  // Estrutura do Cplex (ambiente, modelo e variaveis)
  int _n       = static_cast<int>(GetTaskSize() - 2);  // less source and target
  int _d       = static_cast<int>(GetFileSize());
  int _m       = static_cast<int>(GetVirtualMachineSize());
  int _mb      = static_cast<int>(GetStorageSize());
  int _numr    = static_cast<int>(GetRequirementsSize());
  int _numb    = static_cast<int>(get_bucket_size());
  int _t       = makespan_max_;
  double _cmax = get_budget_max();
  double _smax = get_maximum_security_and_privacy_exposure();

  struct CPLEX cplx(_n, _d, _m, _numr, _numb);

  // variaveis de execucao
  // X_IJT => a tarefa I que esta na maquina J, comeca a executar no periodo T
  for (int i = 0; i < _n; i++) {
    cplx.x[i] =  IloArray<IloBoolVarArray>(cplx.env, _m);
    for (int j = 0; j < _m; j++) {
      cplx.x[i][j] = IloBoolVarArray(cplx.env, _t);
      for (int k = 0; k < _t; k++) {
      sprintf (var_name, "x_%d_%d_%d", (int)i,(int)j, (int)k);              // nome da variavel
      cplx.x[i][j][k] = IloBoolVar(cplx.env, var_name);                     // aloca variavel
      cplx.model.add(cplx.x[i][j][k]);                                      // adiciona variavel ao modelo
      }
    }
  }

  // variaveis de leitura
  // R_IDJPT => a tarefa I que esta na maquina J, comeca a ler o seu D-esimo dado de entrada
  // (note que nao eh o dado de indice D) a partir da maquina P no periodo T
  for (int i = 0; i < _n; i++) {
    Task*              task        = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> input_files = task->get_input_files();
    cplx.r[i]                      =  IloArray<IloArray<IloArray<IloBoolVarArray>>>(cplx.env, static_cast<int>(input_files.size()));

    for (int j = 0; j < static_cast<int>(input_files.size()); j++) {
      cplx.r[i][j] = IloArray<IloArray<IloBoolVarArray>>(cplx.env, _m);
      for (int k = 0; k < _m; k++) {
        cplx.r[i][j][k] = IloArray<IloBoolVarArray>(cplx.env, _mb);
        for (int l = 0; l < _mb; l++) {
          cplx.r[i][j][k][l] = IloBoolVarArray(cplx.env, _t);
          for (int m = 0; m < _t; m++) {
            sprintf (var_name, "r_%d_%d_%d_%d_%d", (int) i, (int) j, (int) k, (int) l, (int) m);      // nome da variavel
            cplx.r[i][j][k][l][m] = IloBoolVar(cplx.env, var_name);                                   // aloca variavel
            cplx.model.add(cplx.r[i][j][k][l][m]);                                                    // adiciona variavel ao modelo
          }
        }
      }
    }
  }

  // variaveis de escrita
  // W_IDJPT => a tarefa I que esta na maquina J, comeca a escrever o seu D-esimo dado de entrada
  // (note que nao eh o dado de indice D) a partir da maquina P no periodo T
  for (int i = 0; i < _n; i++) {
    Task*              task         = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> output_files = task->get_output_files();
    cplx.w[i]                       =  IloArray<IloArray<IloArray<IloBoolVarArray>>>(cplx.env, static_cast<int>(output_files.size()));

    for (int j = 0; j < static_cast<int>(output_files.size()); j++) {
      cplx.w[i][j] = IloArray<IloArray<IloBoolVarArray>>(cplx.env, _m);
      for (int k = 0; k < _m; k++) {
	      cplx.w[i][j][k] = IloArray<IloBoolVarArray>(cplx.env, _mb);
	      for (int l = 0; l < _mb; l++) {
          cplx.w[i][j][k][l] = IloBoolVarArray(cplx.env, _t);
          for (int m = 0; m < _t; m++) {
            sprintf (var_name, "w_%d_%d_%d_%d_%d", (int) i, (int) j, (int) k, (int) l, (int) m);      // nome da variavel
            cplx.w[i][j][k][l][m] = IloBoolVar(cplx.env, var_name);                                   // aloca variavel
            cplx.model.add(cplx.w[i][j][k][l][m]);                                                    // adiciona variavel ao modelo
		      }
		    }
	    }
	  }
  }


  // variaveis de armazenamento
  // Y_DJT => indica se o dado de indice D esta armazenado na maquina J no periodo T
  for (int i = 0; i < _d; i++)
  {
    cplx.y[i] =  IloArray<IloBoolVarArray>(cplx.env, _mb);
    for(int j = 0; j < _mb; j++)
    {
      cplx.y[i][j] = IloBoolVarArray(cplx.env, _t);
      for(int k = 0; k < _t; k++)
      {
        sprintf(var_name, "y_%d_%d_%d", (int) i, (int) j, (int) k);                    // nome da variavel
        cplx.y[i][j][k] = IloBoolVar(cplx.env, var_name);                              // aloca variavel
        cplx.model.add(cplx.y[i][j][k]);                                               // adiciona variavel ao modelo
      }
    }
  }


  // variaveis de armazenamento independente de maquina
  // Y_DJ => indica se o dado de indice D existe em algum tempo na maquina J
  for (int i = 0; i < _d; i++)
  {
    cplx.yb[i] =  IloBoolVarArray(cplx.env, _mb);
    for(int j = 0; j < _mb; j++)
    {
      sprintf(var_name, "yb_%d_%d", (int) i, (int) j);                     // nome da variavel
      cplx.yb[i][j] = IloBoolVar(cplx.env, var_name);                      // aloca variavel
      cplx.model.add(cplx.yb[i][j]);                                       // adiciona variavel ao modelo
    }
  }


  // variaveis de penalidade de violacao de uma aresta soft
  // W_D1D2 => violacao da aresta soft di,d2 \in E_s
  for (int d1=0; d1 < _d-1; d1++)
  {
    cplx.ws[d1] =  IloBoolVarArray(cplx.env, _d);
    for(int d2=d1+1; d2 < _d; d2++)
    {
      int conflict = conflict_graph_.ReturnConflict(static_cast<size_t>(d1), static_cast<size_t>(d2));

      if (conflict > 0) {  // soft constraint
        sprintf(var_name, "ws_%d_%d", (int) d1, (int) d2);                     // nome da variavel
        cplx.ws[d1][d2] = IloBoolVar(cplx.env, var_name);                      // aloca variavel
        cplx.model.add(cplx.ws[d1][d2]);                                       // adiciona variavel ao modelo
      }
    }
  }


  // variaveis de exposição
  // E_RI => nivel de exposicao da tarefa I pelo requerimento R
  for(int r=0; r < _numr; r++)
    {
      cplx.e[r] =  IloNumVarArray(cplx.env, _n);

      for(int i=0; i < _n; i++)
      {
        sprintf (var_name, "xe_%d_%d", (int)r,(int)i);                       // nome da variavel
	      cplx.e[r][i] = IloNumVar(cplx.env, 0, IloInfinity, var_name);       // aloca variavel
	      cplx.model.add(cplx.e[r][i]);                                       // adiciona variavel ao modelo
	    }
    }


  // variaveis de uso dos buckets
  // B_JL => se o bucket J esta sendo usada no intervalo L
  // *OBS* ******* O INTERVALO 0 serve para indicar que o BUCKET NAO ESTA SENDO USADO ******
  for(int b = 0; b < _numb; b++) {
    Storage* storage = GetStoragePerId(GetVirtualMachineSize() + static_cast<size_t>(b));

    if (Bucket* bucket = dynamic_cast<Bucket*>(storage)) {
      cplx.b[b] = IloBoolVarArray(cplx.env, static_cast<int>(bucket->get_number_of_GB_per_cost_intervals())+1);

      for(int l = 0; l <= static_cast<int>(bucket->get_number_of_GB_per_cost_intervals()); l++)
      {
        sprintf (var_name, "b_%d_%d", (int)b,(int)l);                       // nome da variavel
        cplx.b[b][l] = IloBoolVar(cplx.env, var_name);                      // aloca variavel
        cplx.model.add(cplx.b[b][l]);                                       // adiciona variavel ao modelo
      }
    } else {
      exit(1);  // error
    }
  }

  // variaveis de uso dos buckets
  // Q_JL => quantidade de dados usada pelo bucket J no intervalo L
  // *OBS* ******* O INTERVALO 0 serve para indicar que o BUCKET NAO ESTA SENDO USADO ******
  for(int b = 0; b < _numb; b++) {
    Storage* storage = GetStoragePerId(GetVirtualMachineSize() + static_cast<size_t>(b));

    if (Bucket* bucket = dynamic_cast<Bucket*>(storage)) {
      cplx.q[b] = IloNumVarArray(cplx.env, static_cast<int>(bucket->get_number_of_GB_per_cost_intervals())+1);

      for(int l = 0; l <= static_cast<int>(bucket->get_number_of_GB_per_cost_intervals()); l++)
      {
        sprintf(var_name, "q_%d_%d", (int) b, (int) l);                     // nome da variavel
        cplx.q[b][l] = IloNumVar(cplx.env, 0, IloInfinity, var_name);       // aloca variavel
        cplx.model.add(cplx.q[b][l]);                                       // adiciona variavel ao modelo
      }
    } else {
      exit(1);  // error
    }
  }


  // variaveis de uso de maquina por tempo
  // V_JT => indica se a maquina J esta em uso (contratada) no periodo T
  for(int i = 0; i < _m; i++)
  {
    cplx.v[i] =  IloBoolVarArray(cplx.env, _t);
    for(int j=0; j < _t; j++)
    {
      sprintf(var_name, "v_%d_%d", (int) i, (int) j);                     // nome da variavel
      cplx.v[i][j] = IloBoolVar(cplx.env, var_name);                      // aloca variavel
      cplx.model.add(cplx.v[i][j]);                                       // adiciona variavel ao modelo
    }
  }

  // variaveis de tempo total de uso por maquina
  // Z_J => indica tempo total em que a maquina J foi usada (contratada)
  for(int i=0; i < _m; i++)
  {
    sprintf(var_name, "z_%d", (int) i);                         // nome da variavel
    cplx.z[i] = IloNumVar(cplx.env, 0, IloInfinity, var_name);  // aloca variavel
    cplx.model.add(cplx.z[i]);                                  // adiciona variavel ao modelo
  }


  //variaveis de tempo total (makespam)
  // Z_MAX => makespam do workflow
  sprintf(var_name, "z_max");                                     // nome da variavel
  cplx.z_max[0] = IloNumVar(cplx.env, 0, IloInfinity, var_name);  // aloca variavel
  cplx.model.add(cplx.z_max[0]);                                  // adiciona variavel ao modelo

  // ---------------- funcao objetivo -------------------
  IloExpr fo(cplx.env);

  // ---- Makespam
  fo = alpha_time_ * (cplx.z_max[0] / _t);

  // ----- Custo Financeiro
  for (int j = 0; j < _m; j++)
  {
    VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(j));
    fo += alpha_budget_ * ((virtual_machine->get_cost() * cplx.z[j]) / _cmax);
  }

  for (int b = 0; b < _numb; b++)
  {
    Storage* storage = GetStoragePerId(GetVirtualMachineSize() + static_cast<size_t>(b));

    if (Bucket* bucket = dynamic_cast<Bucket*>(storage))
    {
      for (int l = 0; l <= static_cast<int>(bucket->get_number_of_GB_per_cost_intervals()); l++)
      {
        fo += alpha_budget_ * ((bucket->get_cost() * cplx.q[b][l]) / _cmax);
      }
    }
    else
    {
      exit(1);  // error
    }
  }

  // Exposicao
  for (int r = 0; r < _numr; r++)
  {
    for(int i = 0; i < _n; i++)
    {
      fo += alpha_security_ * (cplx.e[r][i] / _smax);
    }
  }

  // Penalidades das Soft arestas
  for (int d1=0; d1 < _d-1; d1++)
  {
    for (int d2=d1+1; d2 < _d; d2++)
    {
      int conflict = conflict_graph_.ReturnConflict(static_cast<size_t>(d1), static_cast<size_t>(d2));

      if (conflict > 0)
      {  // soft constraint
        fo += alpha_security_ * (conflict * cplx.ws[d1][d2] / _smax);
      }
    }
  }

  cplx.model.add(IloMinimize(cplx.env,fo,"fo"));                            // adiciona função objetivo ao modelo
  // -----------------------------------------------------

  // --------------- RESTRIÇÕES ----------
  for (int i = 0; i < _n; i++)
  {
	  IloExpr exp(cplx.env);
	  for (int j = 0; j < _m; j++)
	    for (int t = 0; t < _t; t++)
	      exp += cplx.x[i][j][t];

	  IloConstraint c(exp == 1);
	  sprintf(var_name, "c4_%d", (int) i);
	  c.setName(var_name);
	  cplx.model.add(c);

	  exp.end();
  }

  // Restricao (5)
  for (int i = 0; i < _n; i++)
  {
    Task*              task        = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> input_files = task->get_input_files();

    for (int d = 0; d < static_cast<int>(input_files.size()); d++)
    {
      IloExpr exp(cplx.env);
        for(int j=0; j < _m; j++)
          for(int p=0; p < _mb; p++)
            for(int t=0; t < _t; t++)
              exp +=cplx.r[i][d][j][p][t];

      IloConstraint c(exp == 1);
      sprintf (var_name, "c5_%d_%d", (int)i, (int)d);
      c.setName(var_name);
      cplx.model.add(c);

      exp.end();
    }
  }


  // Restricao (6)
  for (int i = 0; i < _n; i++)
  {
    Task*              task         = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> output_files = task->get_output_files();

    for (int d = 0; d < static_cast<int>(output_files.size()); d++)
    {
      IloExpr exp(cplx.env);
      for (int j = 0; j < _m; j++)
        for (int p = 0; p < _mb; p++)
          for (int t = 0; t < _t; t++)
            exp +=cplx.w[i][d][j][p][t];

      IloConstraint c(exp == 1);
      sprintf (var_name, "c6_%d_%d", (int)i, (int)d);
      c.setName(var_name);
      cplx.model.add(c);

      exp.end();
    }
  }


  // Restricao (7)
  for (int teto, i = 0; i < _n; i++)
  {
    Task*              task         = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> output_files = task->get_output_files();

    for (int d = 0; d < static_cast<int>(output_files.size()); d++)
    {
      for (int j = 0; j < _m; j++)
      {
        VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(j));

        for (int p = 0; p < _mb; p++)
        {
          int tempo = std::ceil(task->get_time() * virtual_machine->get_slowdown());
          for (int t = tempo; t < _t; t++)
          {
            IloExpr exp(cplx.env);
            exp +=cplx.w[i][d][j][p][t];

            /* (q <= teto) pois o tamanho do intervalo é o mesmo não importa se o tempo comeca de 0 ou 1 */
            teto = std::max(0, tempo);
            for(int q = 0; q <= teto; q++)
              exp -=cplx.x[i][j][q];

            IloConstraint c(exp <= 0);
            sprintf (var_name, "c7_%d_%d_%d_%d_%d", (int) i, (int) d, (int) j, (int) p, (int) t);
            c.setName(var_name);
            cplx.model.add(c);

            exp.end();
          }
        }
      }
    }
  }

  // Restricao (8)
  for (int i = 0; i < _n; i++)
  {
    Task*              task         = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> output_files = task->get_output_files();

    for (int d = 0; d < static_cast<int>(output_files.size()); d++)
    {
      for (int j = 0; j < _m; j++)
      {
        VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(j));

        for (int p = 0; p < _mb; p++)
        {
          int tempo = std::ceil(task->get_time() * virtual_machine->get_slowdown());
          for (int t = tempo; t < _t; t++)
          {
            IloExpr exp(cplx.env);
            exp +=cplx.w[i][d][j][p][t];

            IloConstraint c(exp == 0);
            sprintf (var_name, "c8_%d_%d_%d_%d_%d", (int)i, (int)d, (int)j, (int)p, (int)t);
            c.setName(var_name);
            cplx.model.add(c);

            exp.end();
		      }
	      }
  	  }
    }
  }

  //Retricao (9)
  for (int teto, i = 0; i < _n; i++)
  {
    Task*              task        = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> input_files = task->get_input_files();

    for (int d = 0; d < static_cast<int>(input_files.size()); d++)
    {
      File* file = input_files[static_cast<size_t>(d)];

      for (int j = 0; j < _m; j++)
      {
        VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(j));

        /* vamos fazer para todo t pois assim quando (t-t_djp) < 0, o lado direito sera 0 e impede a execucao da tarefa naquele tempo */
        for (int t = 0; t < _t; t++)
        {
          IloExpr exp(cplx.env);
          exp = cplx.x[i][j][t];

          for (int p = 0; p < _mb; p++)
          {
            Storage* storage = GetStoragePerId(static_cast<size_t>(p));
            /* (q <= teto) pois o tamanho do intervalo é o mesmo não importa se o tempo comeca de 0 ou 1 */

            if ((t - ComputeFileTransferTime(file, virtual_machine, storage)) >= 0)
            {
              teto = max(0, t - ComputeFileTransferTime(file, virtual_machine, storage));
              for (int q = 0; q <= teto; q++)
                  exp -= cplx.r[i][d][j][p][q];
            }

          }

          IloConstraint c(exp <= 0);
          sprintf (var_name, "c9_%d_%d_%d_%d", (int)i, (int)d, (int)j, (int)t);
          c.setName(var_name);
          cplx.model.add(c);

          exp.end();
        }
      }
    }
  }



// Restricao (10)
for(int piso,j=0; j < _m; j++)
  {
    VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(j));

    for(int t=0; t < _t; t++)
      {
      IloExpr exp(cplx.env);

      /* execucao */
      for (int i = 0; i < _n; i++)
        {
          Task* task = GetTaskPerId(static_cast<size_t>(i + 1));
          piso       = max(0, t - static_cast<int>((std::ceil(task->get_time() * virtual_machine->get_slowdown()) + 1.0)));
          for(int q=piso; q <= t; q++)
            exp +=cplx.x[i][j][q];
        }

      /* escrita */
      for(int i=0; i < _n; i++)
        {
          Task*              task         = GetTaskPerId(static_cast<size_t>(i + 1));
          std::vector<File*> output_files = task->get_output_files();

          for (int d = 0; d < static_cast<int>(output_files.size()); d++)
            {
              File* file = output_files[static_cast<size_t>(d)];

              for(int p=0; p < _mb; p++)
                {
                  Storage* storage = GetStoragePerId(static_cast<size_t>(p));
                  piso             = max(0, t - ComputeFileTransferTime(file, virtual_machine, storage) + 1);
                  for(int rr=piso; rr <= t; rr++)
                    exp += cplx.w[i][d][j][p][rr];
                }
                  }
        }

      /* leitura */
      for (int i = 0; i < _n; i++)
        {
          Task*              task        = GetTaskPerId(static_cast<size_t>(i + 1));
          std::vector<File*> input_files = task->get_input_files();

          for (int d = 0; d < static_cast<int>(input_files.size()); d++)
            {
              File* file = input_files[static_cast<size_t>(d)];

              for(int p=0; p < _mb; p++)
                {
                  Storage* storage = GetStoragePerId(static_cast<size_t>(p));
                  piso             = max(0, t - ComputeFileTransferTime(file, virtual_machine, storage) + 1);
                  for(int rr=piso; rr <= t; rr++)
                    exp += cplx.r[i][d][j][p][rr];
                }
	      }
	  }

	/* contratacao */
	exp -= cplx.v[j][t];

	IloConstraint c(exp <= 0);
	sprintf (var_name, "c10_%d_%d", (int)j, (int)t);
	c.setName(var_name);
	cplx.model.add(c);

	exp.end();
      }
  }



// Restricao (11)
for(int b = 0; b < _numb; b++)
  {
    Storage* storage = GetStoragePerId(GetVirtualMachineSize() + static_cast<size_t>(b));

    for(int t=0; t < _t; t++)
      {
        IloExpr exp(cplx.env);

        /* escrita */
        for(int i=0; i < _n; i++)
          {
            Task*              task         = GetTaskPerId(static_cast<size_t>(i + 1));
            std::vector<File*> output_files = task->get_output_files();

            for (int d = 0; d < static_cast<int>(output_files.size()); d++)
              {
                File* file = output_files[static_cast<size_t>(d)];

                for(int p=0; p < _m; p++)
                  {
                    VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(p));
                    int piso                        = max(0, t - ComputeFileTransferTime(file, storage, virtual_machine) + 1);
                    for(int rr=piso; rr <= t; rr++)
                      exp += cplx.w[i][d][p][b][rr];
                  }
	              }
	          }

          /* leitura */
          for (int i = 0; i < _n; i++)
            {
              Task*              task        = GetTaskPerId(static_cast<size_t>(i + 1));
              std::vector<File*> input_files = task->get_input_files();

              for (int d = 0; d < static_cast<int>(input_files.size()); d++)
                {
                  File* file = input_files[static_cast<size_t>(d)];

                  for(int p=0; p < _m; p++)
                    {
                      VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(p));
                      int piso                        = max(0, t - ComputeFileTransferTime(file, storage, virtual_machine) + 1);
                      for(int rr=piso; rr <= t; rr++)
                        exp += cplx.r[i][d][p][b][rr];
                    }
                }
            }

          IloConstraint c(exp <= 1);
          sprintf (var_name, "c11_%d_%d", (int)b, (int)t);
          c.setName(var_name);
          cplx.model.add(c);

          exp.end();
      }
  }



  //Restricao (12)
  for(int d = 0; d < static_cast<int>(files_.size()); ++d)
  {
    File* file = files_[static_cast<size_t>(d)];

    if (dynamic_cast<DynamicFile*>(file)) {
      for(int j=0; j < _mb; j++)
      {
        IloExpr exp(cplx.env);
        exp+=cplx.y[d][j][0];

        IloConstraint c(exp == 0);
        sprintf (var_name, "c12_%d_%d", (int)d, (int)j);
        c.setName(var_name);
        cplx.model.add(c);

        exp.end();
      }
    }
  }


  //Restricao (13) e (14)
  for (int d = 0; d < static_cast<int>(files_.size()); ++d)
  {
    File* file = files_[static_cast<size_t>(d)];

    if (StaticFile* static_file = dynamic_cast<StaticFile*>(file))
    {
      int ind = static_cast<int>(static_file->GetFirstVm());

      for (int mb = 0; mb < static_cast<int>(storages_.size()); ++mb)
      {
        if (ind == mb)
        {
          for (int t = 0; t < _t; t++)
          {
            IloExpr exp(cplx.env);
            exp += cplx.y[d][ind][t];

            IloConstraint c(exp == 1);
            sprintf (var_name, "c14_%d_%d_%d", (int)d, (int)ind, (int)t);
            c.setName(var_name);
            cplx.model.add(c);

            exp.end();
          }
        }
        else
        {
          IloExpr exp(cplx.env);
          exp+=cplx.y[d][ind][0];

          IloConstraint c(exp == 0);
          sprintf (var_name, "c13_%d_%d", (int)d, (int)ind);
          c.setName(var_name);
          cplx.model.add(c);

          exp.end();
        }
      }
    }
  }

  // Restricao (15)
  // for (int indw = -1, pai_i = 0, tempo = 0, /* <RODRIGO> para todo dado "d" dinamico */)
  for (size_t i = 0; i < GetFileSize(); ++i)
  {
    if (DynamicFile* dynamic_file = dynamic_cast<DynamicFile*>(files_[i]))
    {
      // pai_i      = /* <RODRIGO> tarefa que escreveu o dado "d" */;
      // indw       = /* <RODRIGO> o dado "d" eh a d-esima escrita de "pai_i" */;
      // File* file = input_files[static_cast<size_t>(d)]; /* <RODRIGO> eh esse d mesmo neh ? */

      Task* pai = dynamic_file->get_parent_task();
      int indw  = static_cast<int>(dynamic_file->get_parent_output_file_index());
      int d     = static_cast<int>(dynamic_file->get_id());
      // Shift por conta da tarefa virtual SOURCE (id: 0)
      int pai_i = static_cast<int>(pai->get_id()) - 1;

      for (int p = 0; p < _mb; p++)
      {
        Storage* storage = GetStoragePerId(static_cast<size_t>(p));

        for (int t = 0; t < _t - 1; t++)
        {
          IloExpr exp(cplx.env);
          // exp += cplx.y[d][p][t+1];
          // exp -= cplx.y[d][p][t];

          exp += cplx.y[d][p][t + 1];
          exp -= cplx.y[d][p][t];

          for (int j = 0; j < _m; j++)
          {
            VirtualMachine* virtual_machine = GetVirtualMachinePerId(static_cast<size_t>(j));
            int tempo                       = (t - ComputeFileTransferTime(dynamic_file, storage, virtual_machine) + 1);
            if (tempo >= 0)
            // if (tempo >= 0 && tempo < _t)
            // if (tempo > 0 && tempo < _t)
            {
              // exp-=cplx.w[pai_i][indw][j][p][tempo];
              exp -= cplx.w[pai_i][indw][j][p][tempo];
            }
          }

          IloConstraint c(exp <= 0);
          // sprintf (var_name, "c15_%d_%d_%d", (int)d, (int)p, (int)t);
          sprintf (var_name, "c15_%d_%d_%d", (int) d, (int) p, (int) t);
          c.setName(var_name);
          cplx.model.add(c);

          exp.end();
        }
      }
    }
  }



// Restricao (16)
for (int dd, i = 0; i < _n; i++)
  {
    Task*              task        = GetTaskPerId(static_cast<size_t>(i + 1));
    std::vector<File*> input_files = task->get_input_files();

    for (int d = 0; d < static_cast<int>(input_files.size()); d++)
      {
      for(int p=0; p < _mb; p++)
        {
          for(int t=0; t < _t; t++)
            {
              IloExpr exp(cplx.env);
              for(int j=0; j < _m; j++)
                exp+= cplx.r[i][d][j][p][t];

              File* file = input_files[static_cast<size_t>(d)];

              dd = file->get_id();
              exp-=cplx.y[dd][p][t];

              IloConstraint c(exp <= 0);
              sprintf (var_name, "c16_%d_%d_%d_%d", (int)i, (int)d, (int)p, (int)t);
              c.setName(var_name);
              cplx.model.add(c);

              exp.end();
            }
        }
      }
  }


  IloCplex solver(cplx.model);                          // declara variável "solver" sobre o modelo a ser solucionado
  // solver.exportModel("model.lp");                       // escreve modelo no arquivo no formato .lp
  solver.exportModel(FLAGS_cplex_output_file.c_str());  // escreve modelo no arquivo no formato .lp
}
