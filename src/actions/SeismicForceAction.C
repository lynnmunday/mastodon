/*************************************************/
/*           DO NOT MODIFY THIS HEADER           */
/*                                               */
/*                     MASTODON                  */
/*                                               */
/*    (c) 2015 Battelle Energy Alliance, LLC     */
/*            ALL RIGHTS RESERVED                */
/*                                               */
/*   Prepared by Battelle Energy Alliance, LLC   */
/*     With the U. S. Department of Energy       */
/*                                               */
/*     See COPYRIGHT for full restrictions       */
/*************************************************/

// MOOSE includes
#include "FEProblemBase.h"
#include "Factory.h"
#include "MooseMesh.h"

// Mastodon includes
#include "SeismicForceAction.h"

registerMooseAction("MastodonApp", SeismicForceAction, "add_bc");

template <>
InputParameters
validParams<SeismicForceAction>()
{
  InputParameters params = validParams<Action>();
  params.addClassDescription("This action sets up Seismic forces boundary conditions.");

  params.addRequiredParam<std::vector<VariableName>>("displacements",
                                                     "The displacement variables for the problem.");
  params.addRequiredParam<std::vector<BoundaryName>>(
      "boundary",
      "The list of boundary IDs from the mesh where this boundary "
      "condition will be applied.");
  params.addRequiredParam<std::vector<unsigned int>>(
      "input_components", "The directions in which the input ground motions are applied.");
  params.addParam<std::vector<VariableName>>(
      "velocities",
      "The vector of velocity variables names. The input "
      "velocities can be obtained from this variable. The size "
      "of this vector must be same size as input_component.");
  params.addParam<std::vector<FunctionName>>("velocity_functions",
                                             "The vector of function names that describes the "
                                             "input ground motions. Must be same size as "
                                             "input_component.");
  params.addParam<Real>(
      "alpha", 0.0, "The alpha parameter required for HHT time integration scheme.");
  params.addRequiredRangeCheckedParam<Real>(
      "density", "density>0.0", "Density of the underlying bedrock.");
  params.addRequiredRangeCheckedParam<Real>(
      "p_wave_speed", "p_wave_speed>0.0", "P-wave speed of the underlying bedrock.");
  params.addRequiredRangeCheckedParam<Real>(
      "shear_wave_speed", "shear_wave_speed>0.0", "shear wave speed of the underlying bedrock.");
  return params;
}

SeismicForceAction::SeismicForceAction(const InputParameters & params)
  : Action(params),
    _boundary(getParam<std::vector<BoundaryName>>("boundary")),
    _displacements(getParam<std::vector<VariableName>>("displacements")),
    _input_components(getParam<std::vector<unsigned int>>("input_components")),
    _velocity_ptrs(isParamValid("velocities") ? &getParam<std::vector<VariableName>>("velocities")
                                              : NULL),
    _velocity_function_ptrs(isParamValid("velocity_functions")
                                ? &getParam<std::vector<FunctionName>>("velocity_functions")
                                : NULL)
{
  if (!_velocity_ptrs && !_velocity_function_ptrs)
    mooseError("A list of functions ('velocity_functions') or variables "
               "('velocities') describing the input velocity must be supplied "
               "in the \"",
               name(),
               "\" block.");

  if (_velocity_ptrs && (_input_components.size() != _velocity_ptrs->size()))
    mooseError("The number of components listed in 'input_components' must "
               "equal the number of variables in 'velocities' in the \"",
               name(),
               "\" block.");

  if (_velocity_function_ptrs && (_input_components.size() != _velocity_function_ptrs->size()))
    mooseError("The number of components listed in 'input_components' must "
               "equal the number of functions in 'velocity_functions' in the "
               "\"",
               name(),
               "\" block.");
}

void
SeismicForceAction::act()
{
  // Cannot be done in constructor because _mesh is not defineds
  if (_displacements.size() != _mesh->dimension())
    mooseError("The number of supplied 'displacements' must equal the mesh "
               "dimension in the \"",
               name(),
               "\" block.");

  // Define parameters object for object to be created
  InputParameters params = _factory.getValidParams("SeismicForce");
  params.set<std::vector<BoundaryName>>("boundary") = _boundary;
  params.set<Real>("alpha") = getParam<Real>("alpha");
  params.set<Real>("density") = getParam<Real>("density");
  params.set<Real>("p_wave_speed") = getParam<Real>("p_wave_speed");
  params.set<Real>("shear_wave_speed") = getParam<Real>("shear_wave_speed");

  // Loop through each component and create SeismicForce object
  std::string prefix = "mastodon_seismic_force";
  for (unsigned int j = 0; j < _input_components.size(); ++j)
  {
    params.set<unsigned int>("vel_component") = _input_components[j];
    if (_velocity_function_ptrs)
      params.set<FunctionName>("velocity_function") = (*_velocity_function_ptrs)[j];
    else
      params.set<std::vector<VariableName>>("velocity") = {(*_velocity_ptrs)[j]};

    for (unsigned int i = 0; i < _displacements.size(); ++i)
    {
      std::stringstream obj_name;
      obj_name << prefix << "_" << name() << "_" << i << "_" << _input_components[j];
      params.set<unsigned int>("component") = i;
      params.set<NonlinearVariableName>("variable") = _displacements[i];

      _problem->addBoundaryCondition("SeismicForce", obj_name.str(), params);
    }
  }
}
