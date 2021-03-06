#pragma once
#include "CommonTools.h"

#include <utility>
#include <vector>
#include <memory>
#include <array>
#include <cassert>
#include <algorithm>
#include <iostream>
#include <string>

namespace SkeletonUpdate
{
	using namespace std;


	struct alignas(4 * sizeof(float)) Matrix4
	{
		static Matrix4 const Identity;

		union
		{
			float m[4][4];
			struct
			{
				float _00, _01, _02, _03;
				float _10, _11, _12, _13;
				float _20, _21, _22, _23;
				float _30, _31, _32, _33;
			};
			__m128 m128s[4];
		};

		Matrix4() = default;


		Matrix4(
			float _00, float _01, float _02, float _03,
			float _10, float _11, float _12, float _13,
			float _20, float _21, float _22, float _23,
			float _30, float _31, float _32, float _33)
			: _00(_00), _01(_01), _02(_02), _03(_03)
			, _10(_00), _11(_01), _12(_02), _13(_03)
			, _20(_00), _21(_01), _22(_02), _23(_03)
			, _30(_00), _31(_01), _32(_02), _33(_03)
		{
		}

		friend Matrix4 operator*(Matrix4 const& left, Matrix4 const& right)
		{
			Matrix4 result;
			for (size_t r = 0; r < 4; r++)
			{
				for (size_t c = 0; c < 4; c++)
				{
					for (size_t i = 0; i < 4; i++)
					{
						result.m[r][c] = left.m[r][i] * right.m[i][r];
					}
				}
			}
			return result;
		}
	};
	Matrix4 const Matrix4::Identity = Matrix4(
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	);


	size_t const SkeletonCount = 100;
	size_t const InstancePerSkeletonCount = 10;
	size_t const UpdateCount = 10;

	size_t const OtherDataFloatSize = 64;

	auto JointCountForSkeleton(size_t id)
	{
		return (id + 10) * (id + 10) / 10;
	}
	auto ParentIndexForJoint(size_t index)
	{
		return index / 2;
	}

	namespace V0
	{
		struct Joint
		{
			string name;
			size_t parent;

			array<float, OtherDataFloatSize> someCommonData;

			Matrix4 local;
			Matrix4 model;

			Joint()
				: local(Matrix4::Identity), model(Matrix4::Identity)
			{
			}
		};

		struct Skeleton
		{
			string name;
			vector<unique_ptr<Joint>> joints;

			void UpdateModelMatrices()
			{
				for (auto& joint : joints)
				{
					size_t parent = joint->parent;
					if (parent != size_t(-1))
					{
						joint->model = joints[parent]->model * joint->local;
					}
				}
			}
		};

	}

	namespace V1
	{
		struct Joint
		{
			string name;
			size_t parent;

			array<float, OtherDataFloatSize> someCommonData;
		};

		struct InstanceJoint
		{
			Matrix4 local;
			Matrix4 model;

			InstanceJoint()
				: local(Matrix4::Identity), model(Matrix4::Identity)
			{
			}
		};

		struct Skeleton
		{
			string name;
			vector<unique_ptr<Joint>> joints;
		};

		struct Instance
		{
			shared_ptr<Skeleton> skeleton;
			vector<unique_ptr<InstanceJoint>> joints;

			void UpdateModelMatrices()
			{
				auto& skeletonJoints = skeleton->joints;
				for (size_t i = 0; i < skeletonJoints.size(); i++)
				{
					size_t parent = skeletonJoints[i]->parent;
					if (parent != size_t(-1))
					{
						joints[i]->model = joints[parent]->model * joints[i]->local;
					}
				}
			}
		};

	}


	namespace V2
	{
		struct Skeleton;
		struct Pose
		{
			shared_ptr<Skeleton> skeleton;
			vector<Matrix4> transformations;
		};

		struct LocalPose : Pose
		{
		};

		struct ModelPose : Pose
		{
		};


		struct Joint
		{
			string name;
			size_t parent;

			array<float, OtherDataFloatSize> someCommonData;
		};

		struct Skeleton
		{
			string name;
			vector<Joint> joints;
		};

		struct Instance
		{
			LocalPose local;
			ModelPose model;

			void UpdateModelMatrices()
			{
				auto& joints = model.skeleton->joints;
				for (size_t i = 0; i < model.transformations.size(); i++)
				{
					size_t parent = joints[i].parent;
					if (parent != size_t(-1))
					{
						model.transformations[i] = model.transformations[parent] * local.transformations[i];
					}
				}
			}
		};

	}

	namespace V0
	{
		struct Tester
		{
			vector<shared_ptr<Skeleton>> skeletons;
			Tester()
			{
				for (size_t s = 0; s < SkeletonCount; s++)
				{
					for (size_t i = 0; i < InstancePerSkeletonCount; i++)
					{
						auto skeleton = make_shared<Skeleton>();

						skeleton->name = "Version 0 - Skeleton for Performance Test, Instance: " + to_string(s);
						size_t jointCount = JointCountForSkeleton(s);
						skeleton->joints.reserve(jointCount);

						skeleton->joints.push_back(make_unique<Joint>());
						auto& root = *skeleton->joints.back();
						root.name = "Version 0 - Joint for Performance Test, Skeleton Instance: " + to_string(s) + ", Joint Index: 0";
						root.parent = size_t(-1);

						for (size_t j = 1; j < jointCount; j++)
						{
							skeleton->joints.push_back(make_unique<Joint>());
							auto& joint = *skeleton->joints.back();
							joint.name = "Version 0 - Joint for Performance Test, Skeleton Instance: " + to_string(s) + ", Joint Index: " + to_string(j);
							joint.parent = ParentIndexForJoint(j);
						}

						skeletons.push_back(skeleton);
					}
				}
			}

			auto Run()
			{
				return MeasureExecutionTime([this]
				{
					for (size_t i = 0; i < UpdateCount; i++)
					{
						for (auto& instance : skeletons)
						{
							instance->UpdateModelMatrices();
						}
					}
				});
			}
		};
	}

	namespace V1
	{
		struct Tester
		{
			vector<shared_ptr<Skeleton>> skeletons;
			vector<shared_ptr<Instance>> instances;

			Tester()
			{
				for (size_t s = 0; s < SkeletonCount; s++)
				{
					auto skeleton = make_shared<Skeleton>();

					skeleton->name = "Version 1 - Skeleton for Performance Test, Instance: " + to_string(s);
					size_t jointCount = JointCountForSkeleton(s);
					skeleton->joints.reserve(jointCount);

					skeleton->joints.push_back(make_unique<Joint>());
					auto& root = *skeleton->joints.back();
					root.name = "Version 1 - Joint for Performance Test, Skeleton Instance: " + to_string(s) + ", Joint Index: 0";
					root.parent = size_t(-1);

					for (size_t j = 1; j < jointCount; j++)
					{
						skeleton->joints.push_back(make_unique<Joint>());
						auto& joint = *skeleton->joints.back();
						joint.name = "Version 1 - Joint for Performance Test, Skeleton Instance: " + to_string(s) + ", Joint Index: " + to_string(j);
						joint.parent = ParentIndexForJoint(j);
					}

					skeletons.push_back(skeleton);
				}

				for (size_t s = 0; s < SkeletonCount; s++)
				{
					for (size_t i = 0; i < InstancePerSkeletonCount; i++)
					{
						auto instance = make_shared<Instance>();
						instance->skeleton = skeletons[s];
						instance->joints.resize(skeletons[s]->joints.size());
						for (auto& joint : instance->joints)
						{
							joint = make_unique<InstanceJoint>();
						}

						instances.push_back(instance);
					}
				}
			}

			auto Run()
			{
				return MeasureExecutionTime([this]
				{
					for (size_t i = 0; i < UpdateCount; i++)
					{
						for (size_t index = 0; index < instances.size(); index++)
						{
							auto& instance = instances[index];
							instance->UpdateModelMatrices();
						}
					}
				});
			}
		};
	}

	namespace V2
	{
		struct Tester
		{
			vector<shared_ptr<Skeleton>> skeletons;
			vector<shared_ptr<Instance>> instances;

			Tester()
			{
				for (size_t s = 0; s < SkeletonCount; s++)
				{
					auto skeleton = make_shared<Skeleton>();

					skeleton->name = "Version 1 - Skeleton for Performance Test, Instance: " + to_string(s);
					size_t jointCount = JointCountForSkeleton(s);
					skeleton->joints.reserve(jointCount);

					skeleton->joints.push_back(Joint());
					auto& root = skeleton->joints.back();
					root.name = "Version 1 - Joint for Performance Test, Skeleton Instance: " + to_string(s) + ", Joint Index: 0";
					root.parent = size_t(-1);

					for (size_t j = 1; j < jointCount; j++)
					{
						skeleton->joints.push_back(Joint());
						auto& joint = skeleton->joints.back();
						joint.name = "Version 1 - Joint for Performance Test, Skeleton Instance: " + to_string(s) + ", Joint Index: " + to_string(j);
						joint.parent = ParentIndexForJoint(j);
					}

					skeletons.push_back(skeleton);
				}

				for (size_t s = 0; s < SkeletonCount; s++)
				{
					for (size_t i = 0; i < InstancePerSkeletonCount; i++)
					{
						auto instance = make_shared<Instance>();
						instance->local.skeleton = skeletons[s];
						instance->local.transformations.resize(skeletons[s]->joints.size(), Matrix4::Identity);
						instance->model.skeleton = skeletons[s];
						instance->model.transformations.resize(skeletons[s]->joints.size(), Matrix4::Identity);

						instances.push_back(instance);
					}
				}
			}

			auto Run()
			{
				return MeasureExecutionTime([this]
				{
					for (size_t i = 0; i < UpdateCount; i++)
					{
						for (size_t index = 0; index < instances.size(); index++)
						{
							auto& instance = instances[index];
							instance->UpdateModelMatrices();
						}
					}
				});
			}
		};
	}


	void Run()
	{
		V0::Tester t0;

		auto v0Time = t0.Run();

		V1::Tester t1;

		auto v1Time = t1.Run();


		V2::Tester t2;

		auto v2Time = t2.Run();

		cout << "v0 time: " << v0Time.count() << ", v1 time: " << v1Time.count() << ", v2 time: " << v2Time.count() << endl;
	}
}